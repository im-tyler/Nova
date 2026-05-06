# Motion Matching Research Notes — Project Kinetic

Source: orangeduck/Motion-Matching (Daniel Holden / Ubisoft La Forge)
Reference location: /Users/tyler/Documents/animation/reference/Motion-Matching/

---

## 1. How the Motion Database Is Built

### Raw Data Pipeline

Source data is BVH (Biovision Hierarchy) motion capture files from the Ubisoft LaForge Animation Dataset. Three clips are used: idle, running, walking. Each clip is processed normally and mirrored (swapping left/right joints), doubling the dataset.

Processing steps in `generate_database.py`:
1. Load BVH files, extract joint hierarchy, positions, rotations
2. Mirror each clip across the YZ plane (swap left/right limbs)
3. Supersample to 60fps using cubic interpolation, simultaneously speed up motion by 10%
4. Create a "simulation bone" (root) from Spine2 position + Hips forward direction — all other bones become local to this root
5. Compute velocities and angular velocities via central finite differences at 60fps
6. Detect foot contacts: toe velocity < 0.15 m/s threshold, smoothed with median filter

### Binary Database Format (`database.bin`)

Written sequentially as flat arrays with shape headers:

```
[nframes, nbones] bone_positions    (vec3, float32)
[nframes, nbones] bone_velocities   (vec3, float32)
[nframes, nbones] bone_rotations    (quat, float32)
[nframes, nbones] bone_angular_velocities (vec3, float32)
[nbones]          bone_parents      (int32)
[nranges]         range_starts      (int32)
[nranges]         range_stops       (int32)
[nframes, ncontacts] contact_states (bool/uint8)
```

Ranges define contiguous clips within the flat frame array. This prevents the search from crossing clip boundaries.

### Feature Vector Construction

Built at runtime by `database_build_matching_features()`. Total feature dimensionality: **27 floats**.

| Feature | Dimensions | Description |
|---------|-----------|-------------|
| Left foot position | 3 | Bone position relative to root, in root space |
| Right foot position | 3 | Same |
| Left foot velocity | 3 | Bone velocity in root space |
| Right foot velocity | 3 | Same |
| Hip velocity | 3 | Hip bone velocity in root space |
| Trajectory positions (2D) | 6 | Future root positions at +20, +40, +60 frames (xz only) |
| Trajectory directions (2D) | 6 | Future root facing directions at +20, +40, +60 frames (xz only) |

Each feature group is independently **normalized**: subtract mean, divide by (average std / weight). The weight parameter controls relative importance during search — higher weight = lower scale = larger contribution to distance. Default weights:
- Foot position: 0.75
- Foot velocity: 1.0
- Hip velocity: 1.0
- Trajectory positions: 1.0
- Trajectory directions: 1.5

Features are stored as a 2D array `[nframes, 27]` alongside offset and scale arrays for normalization/denormalization.

---

## 2. How the Runtime Search Works

### Algorithm: Brute-Force with AABB Acceleration

The search is a **linear scan** over all frames in the database, accelerated by a **two-level axis-aligned bounding box (AABB) hierarchy**. There is no KD-tree.

#### Acceleration Structure

Frames are grouped into fixed-size blocks:
- **Small boxes**: every 16 frames (`BOUND_SM_SIZE = 16`)
- **Large boxes**: every 64 frames (`BOUND_LR_SIZE = 64`)

For each box at each level, the min and max feature values across all contained frames are precomputed. This creates per-dimension AABBs in the 27D feature space.

Built by `database_build_bounds()` — a single pass over all frames, tracking min/max per feature dimension per block.

#### Search Procedure (`motion_matching_search`)

```
For each range (clip):
  i = range_start
  While i < range_end:
    1. Compute distance from query to LARGE bounding box (64-frame block)
       - Distance = sum of squared distances from query to nearest point on box per dimension
       - Early-out: if partial sum exceeds best_cost, break inner loop
       - If box distance >= best_cost: skip to next large box (i += up to 64)

    2. Within large box, check SMALL bounding boxes (16-frame blocks)
       - Same AABB distance check with early-out
       - If box distance >= best_cost: skip to next small box (i += up to 16)

    3. Within small box, check individual frames
       - Skip frames within ±20 of current frame (ignore_surrounding)
       - Compute L2 squared distance: sum(squared(query[j] - features[i][j]))
       - Early-out per dimension if partial cost exceeds best
       - Add transition_cost to distance
       - Update best if lower
```

Additional rules:
- Last 20 frames of each range are excluded from search (`ignore_range_end = 20`) to avoid matching near clip boundaries
- A `transition_cost` penalty is added to all candidates to create hysteresis — the current frame must be beaten by at least this margin

#### Search Frequency

Search does not run every frame. It runs on a timer (`search_time = 0.1s`, i.e., every 6 frames at 60fps). Between searches, the system simply advances `frame_index++` through the database. A forced search triggers when the desired velocity or rotation changes sharply (exceeding a threshold on the delta).

### Complexity

- **Worst case**: O(N * D) where N = total frames, D = 27 features
- **Typical case**: The two-level AABB pruning skips large chunks. When the current pose is a good match, most large boxes are pruned. The early-out on per-dimension accumulation further cuts work.
- **Memory**: Features array is `N * 27 * 4 bytes`. Bounds are `(N/16 + N/64) * 27 * 4 * 2 bytes` (min + max). For a dataset of ~15K frames: ~1.6 KB for features, ~30 KB for bounds. Trivially small.

---

## 3. Data Format for Motion Clips

### Storage

All data is in **local bone space** relative to the parent joint, except bone 0 (root/simulation bone) which is in world space.

Per frame, per bone:
- `vec3 position` — local translation relative to parent
- `quat rotation` — local rotation relative to parent (unit quaternion, xyzw)
- `vec3 velocity` — local translational velocity
- `vec3 angular_velocity` — local angular velocity (scaled axis)

Contact states are per-frame booleans for left and right foot.

### Character Data (`character.bin`)

Separate from animation data. Contains:
- Mesh: positions, normals, texture coordinates, triangle indices
- Skinning: bone weights (4 per vertex), bone indices (4 per vertex)
- Rest pose: bone positions and rotations in rest configuration

### Source Format

Raw data comes from BVH files. The `generate_database.py` script uses a custom BVH parser (`bvh.py`) and quaternion library (`quat.py`). Output is flat binary arrays written with numpy's `tofile()`.

---

## 4. How Transitions and Blending Work

### Inertialization (Not Crossfade Blending)

Motion matching does **not** use crossfade blending between clips. Instead it uses **inertialization** — an offset-decay technique from David Bollo (GDC 2016), refined by Holden.

#### Concept

When a transition occurs (search finds a better frame than the current one):
1. Compute the **offset** between the current pose and the new target pose (position and rotation per bone)
2. Store offset + velocity at the moment of transition
3. Each frame, **decay** the offset toward zero using a critically-damped spring (`decay_spring_damper_exact`)
4. Final pose = new animation pose + decaying offset

This produces smooth transitions without needing to blend two full poses simultaneously. There is zero blending window — the switch is instantaneous, with the offset handling all smoothness.

#### Implementation Details

**Transition initiation** (`inertialize_pose_transition`):
```
For each bone:
  offset_position = (src_position + current_offset) - dst_position
  offset_velocity = (src_velocity + current_offset_vel) - dst_velocity
  // Same for rotations using quaternion multiplication
```

**Per-frame update** (`inertialize_pose_update`):
```
For each bone:
  decay_spring_damper_exact(offset_position, offset_velocity, halflife, dt)
  final_position = input_position + offset_position
  // Same for rotations
```

The root bone is handled specially — it must be transformed from the source clip's coordinate space into the destination clip's coordinate space using `transition_src/dst_position/rotation`.

**Halflife**: 0.1 seconds by default. This means the offset decays to half its magnitude in 0.1s, and is effectively gone in ~0.5s.

#### Spring Damper

The decay spring is a critically-damped spring with no goal (goal = 0):
```
y = halflife_to_damping(halflife) / 2.0
j1 = v + x * y
eydt = fast_negexpf(y * dt)
x = eydt * (x + j1 * dt)
v = eydt * (v - j1 * y * dt)
```

`fast_negexpf` is an approximation: `1 / (1 + x + 0.48x^2 + 0.235x^3)`.

### Simulation Layer Synchronization

There is a separate "simulation object" (position + rotation + velocity) that represents where the character *should* be based on player input. The animation character is pulled toward the simulation object via:

1. **Adjustment**: Damped spring pulling character toward simulation position/rotation, optionally capped by character velocity to prevent sliding
2. **Clamping**: Hard limit — if character drifts more than 0.15m or 90 degrees from simulation, snap to boundary

This two-layer approach (simulation object + animated character) prevents the animation from fighting the controller.

---

## 5. Key Algorithms and Their Complexity

### Motion Matching Search
- **Type**: Brute-force linear scan with two-level AABB pruning
- **Complexity**: O(N * D) worst case, typically much better due to pruning
- **Search rate**: Every ~6 frames (0.1s timer), not every frame

### Feature Normalization
- **Type**: Z-score normalization per feature group with shared std across dimensions
- **Complexity**: O(N * D) one-time precomputation

### Inertialization
- **Type**: Critically-damped spring decay per bone per frame
- **Complexity**: O(B) per frame where B = number of bones (~23)

### Forward Kinematics
- **Type**: Recursive parent-chain traversal or single-pass sorted iteration
- **Complexity**: O(B) per frame

### Trajectory Prediction
- **Type**: Spring-damped simulation projected forward 4 steps (20, 40, 60 frames ahead)
- **Complexity**: O(1) per frame (constant number of prediction steps)

### Contact/IK System
- **Type**: Two-bone IK for foot locking, inertialized contact transitions
- **Complexity**: O(1) per contact bone per frame

### Learned Motion Matching (LMM) — Alternative Path
Replaces the database search + playback with three neural networks:
- **Decompressor**: features + latent (32D) -> full pose. Architecture: 1 hidden layer, 512 units, ReLU. O(512 * input + 512 * output) per eval.
- **Stepper**: features + latent -> velocity of features + latent. Architecture: 2 hidden layers, 512 units each, ReLU. Runs every frame to advance the state.
- **Projector**: features -> projected features + latent. Architecture: 4 hidden layers, 512 units each, ReLU. Replaces the database search entirely.

LMM trades database memory for network weight memory and replaces O(N*D) search with O(1) forward passes through fixed-size networks.

---

## 6. How This Could Map to GPU Compute for Crowd Scenarios

### Standard Motion Matching on GPU

**Parallelism opportunity**: The brute-force search is embarrassingly parallel per-character. Each character's search is independent.

**Approach 1 — Per-character parallel search**:
- Each character gets a thread group / workgroup
- Within the group, threads cooperatively scan the database
- Reduction to find minimum cost across threads
- AABB pruning still works: coarse pass on large boxes, then fine pass
- Feature vectors (27 floats) fit comfortably in shared memory
- Database features can live in a storage buffer, read-only

**Approach 2 — Batched search with shared database**:
- Single database in GPU memory (N * 27 * 4 bytes — tiny)
- Dispatch one workgroup per character, threads within scan different frames
- Use subgroup operations (wave intrinsics) for fast min-reduction
- AABB bounds in shared memory for the workgroup

**Approach 3 — LMM on GPU (best for crowds)**:
- Neural network forward passes are matrix multiplications — GPU native
- 512-wide hidden layers map perfectly to GPU warps/wavefronts
- No database memory needed, just network weights (~few MB)
- Each character: stepper (every frame) + projector (every ~6 frames) + decompressor (every frame)
- Batch all characters into single large matrix multiplications
- Estimated: 1000+ characters feasible on modern GPU with LMM

**Skinning**: Already GPU-native. Linear blend skinning is a per-vertex parallel operation.

**Inertialization**: Per-bone spring decay is trivially parallel across characters.

**Memory budget per character** (standard MM):
- Query vector: 27 * 4 = 108 bytes
- Current pose state: ~23 bones * (3+3+4+3) * 4 = ~1.2 KB
- Offset state for inertialization: ~1.2 KB
- Total: ~2.5 KB per character (excluding shared database)

**Memory budget per character** (LMM):
- Features + latent: (27 + 32) * 4 = 236 bytes
- Network intermediates: 512 * 4 * max_layers = ~8 KB (can be shared via batching)
- Pose output: ~1.2 KB
- Total: ~1.5 KB per character + shared weights (~3 MB)

### Key Bottleneck for Crowds

The bottleneck is not the search or the network — it is **skinning and rendering**. Each character has a unique pose requiring unique vertex transformations. LOD systems, instanced rendering with per-instance bone matrices, and mesh simplification at distance become the real problems at crowd scale.

---

## 7. Estimated Integration Complexity for Godot

### What Godot Provides

- Skeleton3D node with bone transforms
- AnimationPlayer for clip playback (not directly useful for MM)
- GDExtension (C++ native modules) for performance-critical code
- Compute shader support via RenderingDevice

### Integration Strategy

**Phase 1 — Core Motion Matching (2-3 weeks)**:
- Port `database.h` search logic to a GDExtension (C++)
- Load binary database from resource files
- Implement feature computation matching the database format
- Wire up to Skeleton3D: set bone poses directly each frame via `set_bone_pose_position/rotation`
- Implement inertialization in the extension
- Skip Godot's AnimationPlayer entirely — drive bones directly

**Phase 2 — Character Controller (1-2 weeks)**:
- Implement the simulation object (position/velocity/rotation) as a CharacterBody3D or custom node
- Trajectory prediction from player input
- Query construction from simulation state + trajectory
- Adjustment/clamping between simulation and animated character

**Phase 3 — IK and Polish (1 week)**:
- Two-bone IK for foot locking (Godot has SkeletonIK3D but custom is cleaner)
- Contact detection from database contact states
- Look-at IK for head tracking

**Phase 4 — GPU Crowds via LMM (2-4 weeks)**:
- Port network weights to GPU storage buffers
- Write compute shaders for stepper/decompressor/projector forward passes
- Batch evaluation across all crowd characters
- Output bone transforms to per-instance SSBOs for instanced skeletal rendering
- This requires a custom rendering path — Godot's built-in Skeleton3D won't handle thousands of instances efficiently

### Key Risks

1. **Data format**: The Ubisoft LaForge dataset is CC-BY-NC-ND — cannot be used in a commercial product. Need to generate own motion data or use a permissive dataset.
2. **Godot Skeleton3D performance**: Setting bone transforms from GDExtension is fine for tens of characters. For hundreds+, need to bypass Godot's skeleton system and go to GPU directly.
3. **Animation retargeting**: The database is built for a specific skeleton. Retargeting to different character meshes requires bone mapping and possibly re-computing features.
4. **Training pipeline**: LMM requires PyTorch training. The trained weights are exported as flat binary files compatible with the C++ inference code. This pipeline works but needs setup.

### Estimated Total: 6-10 weeks for a production-quality single-character system, additional 2-4 weeks for GPU crowd support.

---

## Appendix: File Map

| File | Purpose |
|------|---------|
| `controller.cpp` | Main demo: game loop, input, rendering, all systems wired together |
| `database.h` | Motion database struct, feature computation, AABB bounds, brute-force search |
| `lmm.h` | Learned Motion Matching: decompressor, stepper, projector evaluation |
| `nnet.h` | Minimal feed-forward neural network (load, evaluate) |
| `spring.h` | Spring dampers, inertialization transition/update functions |
| `character.h` | Mesh/skeleton loading, linear blend skinning |
| `common.h` | Math helpers (clamp, lerp, fast_negexp, fast_atan) |
| `vec.h` | vec3 math |
| `quat.h` | Quaternion math |
| `array.h` | 1D/2D array containers with binary I/O |
| `resources/generate_database.py` | BVH -> binary database pipeline |
| `resources/train_decompressor.py` | Train decompressor + compressor networks |
| `resources/train_stepper.py` | Train stepper network |
| `resources/train_projector.py` | Train projector network |
