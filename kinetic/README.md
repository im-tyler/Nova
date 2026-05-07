# Project Kinetic

Motion matching system for Godot 4.4+. A GDScript editor plugin that imports BVH motion-capture files, builds a searchable motion database with 27-float feature vectors, and drives a `Skeleton3D` at runtime using brute-force nearest-neighbor search with inertialized transitions.

**Status: working prototype.** The plugin is functional end-to-end — BVH import, feature vector construction, search, inertialization, runtime playback. Significant limitations apply at scale (see below).

## Layout

```
kinetic/
  kinetic-plugin/      -- the editor plugin (GDScript)
    addons/kinetic/    -- the plugin code itself
    test-project/      -- standalone Godot test project
  reference/           -- (gitignored) source motion capture data
  test-data/           -- small BVH walk clip for testing
  _pre-consolidation/  -- original PROJECT_PLAN, MOTION_MATCHING_NOTES
```

The plugin's own README ([`kinetic-plugin/addons/kinetic/README.md`](./kinetic-plugin/addons/kinetic/README.md)) covers install, node properties, and usage in detail. This top-level README covers concept, status, and plan.

## Concept

Godot has `AnimationTree` with state machines and blend trees, which is solid for basic animation. Kinetic adds the things `AnimationTree` doesn't have:

- **Motion matching** — data-driven animation selection from motion-capture databases.
- **Inertialized transitions** — pop-free transitions without crossfading.
- (Roadmap) full-body IK, runtime retargeting, dynamic bone chains.

## Plan

### Tier 1 — Procedural Animation (planned)
Full-body IK (FABRIK or similar), physics-driven animation (ragdoll blending, hit reactions), runtime pose modification (look-at, foot placement), dynamic bone chains. Likely requires a C++ port for performance.

### Tier 2 — Motion Matching (working prototype)
- Motion database format and BVH builder. **Working.**
- Brute-force motion matching search with weighted L2 distance. **Working.**
- Inertialized transitions via critically-damped springs. **Working.**
- Editor tools for motion database inspection. **Basic dock present.**
- GPU-accelerated search for crowd scenarios (100+ characters). **Planned.**

### Tier 3 — Advanced
Runtime retargeting across skeleton proportions, visual scripting for skeletal manipulation (Control Rig equivalent), facial animation system.

## Architecture

GDExtension delivery is the long-term plan. The current implementation is GDScript — easier iteration but slower at scale. A C++ port will be needed for databases over ~50k frames.

The motion matching algorithm follows Daniel Holden's (Ubisoft La Forge) reference implementation closely:

1. BVH parsing into bone hierarchies (positions, rotations, channels).
2. Feature vector construction — 27 floats covering left/right foot position and velocity, hip velocity, and future trajectory at +20/+40/+60 frames.
3. Z-score normalization with per-dimension weighting (foot pos 0.75, foot vel 1.0, hip vel 1.0, trajectory pos 1.0, trajectory dir 1.5).
4. Brute-force nearest-neighbor search with hysteresis window (20 frames) and transition cost penalty.
5. Inertialized transitions — current pose offset decays exponentially via critically-damped springs.

## Phase 0 Status

- [x] Study orangeduck/Motion-Matching implementation in detail.
- [x] Define motion database format (compatible with BVH).
- [x] Prototype motion matching search on CPU (single character).
- [x] Implement inertialized transitions.
- [ ] Benchmark search cost vs animation database size.
- [ ] Assess GPU-accelerated search for crowds.
- [ ] Prototype FABRIK IK integration with `Skeleton3D`.
- [ ] Prototype ragdoll blend (`AnimationTree` -> Jolt ragdoll -> blend back).
- [ ] Define `AnimationTree` node interface for new node types.

Exit criteria: motion matching works for a single character with basic locomotion (done); IK foot placement works on uneven terrain (planned); ragdoll blend transitions look acceptable (planned).

## Current Limitations

- **Brute-force search.** Linear scan over all frames. Acceptable up to ~50k frames on modern hardware. No KD-tree, VP-tree, or GPU search.
- **GDScript performance.** Feature computation and search are pure GDScript. Large databases (>100k frames) need a native C++ port.
- **Single clip support.** The database can store multiple clip ranges, but the BVH importer produces one range per file. Multi-clip workflows require manual database merging.
- **No foot locking / IK.** Foot sliding is reduced by motion matching but not eliminated.
- **No animation events.** No support for triggering events (sounds, VFX) at specific frames.
- **Bone map rebuilt every frame.** The database-to-skeleton bone-index mapping is recomputed each frame rather than cached.
- **BVH only.** No FBX, glTF, or Godot Animation import path.

## Reference Material

The original [`MOTION_MATCHING_NOTES.md`](./_pre-consolidation/MOTION_MATCHING_NOTES.md) is preserved in `_pre-consolidation/`. It walks through orangeduck/Motion-Matching's database build pipeline, feature-vector design, search loop, and inertialization in detail — useful when porting this code to C++ or when extending the feature vector.

## References

- orangeduck/Motion-Matching — canonical reference: https://github.com/orangeduck/Motion-Matching
- Open-Source-Motion-Matching-System — Unreal sample reimplemented: https://github.com/dreaw131313/Open-Source-Motion-Matching-System
- Mesh2Motion — open-source Mixamo alternative: https://gamefromscratch.com/mesh2motion-open-source-mixamo-alternative/
- Daniel Holden's publications on motion matching and inertialization.
- GDC Motion Matching talks (Ubisoft, Naughty Dog).
- Godot `Skeleton3D`: https://docs.godotengine.org/en/stable/classes/class_skeleton3d.html
- Godot `AnimationTree`: https://docs.godotengine.org/en/stable/classes/class_animationtree.html

## Test Data

`kinetic/test-data/walk.bvh` is a small walk-cycle BVH for testing the importer. The reference dataset (Ubisoft LaForge Animation Dataset) is **not** vendored — it has its own license. Clone it separately if you need it.
