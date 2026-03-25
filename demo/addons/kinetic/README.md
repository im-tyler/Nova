# Project Kinetic

Motion matching system for Godot 4.4+. A GDScript EditorPlugin that imports BVH motion capture files, builds a searchable motion database with 27-float feature vectors, and drives a Skeleton3D at runtime using brute-force nearest-neighbor search with inertialized transitions.

Part of the Godot-Unreal Parity Initiative.

## Install

1. Copy the `kinetic/` directory into your project's `addons/` folder:

```
your_project/
  addons/
    kinetic/
      plugin.cfg
      kinetic_plugin.gd
      bvh_importer.gd
      motion_database.gd
      motion_matching_player.gd
      spring_utils.gd
```

2. In Godot, go to Project > Project Settings > Plugins.
3. Enable "Kinetic".

The plugin adds a "Kinetic" dock (upper-right by default) with BVH import controls and database inspection.

## Components

### BVHImporter

Parses standard BVH files into a MotionDatabase resource. Handles arbitrary joint hierarchies, 3- and 6-channel joints, and any rotation channel order.

```gdscript
var importer := BVHImporter.new()
var db := importer.import_file("res://mocap/walk.bvh", true, true)
# auto_build_features=true, compute_velocities=true
ResourceSaver.save(db, "res://mocap/walk.tres")
```

The importer auto-detects left foot, right foot, and hip bones by name pattern matching (e.g., "LeftFoot", "Hips", "Pelvis").

### MotionDatabase

Resource that stores all motion data in flat PackedFloat32Array arrays, indexed as `[frame * bone_count + bone]`.

**Stored data per frame:**
- Bone local positions (Vector3 per bone)
- Bone local rotations (Quaternion per bone)
- Bone local velocities (computed via central finite differences)
- Bone local angular velocities
- Root world position, direction, velocity

**Feature vector layout (27 floats per frame):**

| Index | Content |
|-------|---------|
| 0-2 | Left foot position (root space) |
| 3-5 | Right foot position (root space) |
| 6-8 | Left foot velocity (root space) |
| 9-11 | Right foot velocity (root space) |
| 12-14 | Hip velocity (root space) |
| 15-20 | Trajectory positions at +20/+40/+60 frames (xz pairs) |
| 21-26 | Trajectory directions at +20/+40/+60 frames (xz pairs) |

Features are z-score normalized with per-dimension weighting. Trajectory directions are weighted 1.5x by default, body features at 0.75-1.0x.

**Search** is a brute-force linear scan with weighted L2 distance and early-out pruning. A hysteresis window of 20 frames around the current playback position prevents oscillation. Transition cost penalty ensures the current clip is only abandoned when a significantly better match exists.

```gdscript
var best_frame := db.query_match(
    foot_l_pos, foot_r_pos,
    foot_l_vel, foot_r_vel,
    hip_vel,
    traj_positions,  # Array[Vector3], 3 future points
    traj_directions,  # Array[Vector3], 3 future directions
    current_frame
)
```

### MotionMatchingPlayer

Runtime node that drives a Skeleton3D. Attach as a child or sibling of your character scene.

**Exported properties:**

| Property | Default | Description |
|----------|---------|-------------|
| `database` | -- | MotionDatabase resource (must have features built) |
| `skeleton_path` | -- | NodePath to the target Skeleton3D |
| `search_interval` | 6 | Physics frames between searches (~0.1s at 60fps) |
| `inertialization_halflife` | 0.1 | Transition decay halflife in seconds |
| `move_speed` | 3.5 | Movement speed for trajectory prediction (m/s) |
| `turn_speed` | 8.0 | Rotation speed for trajectory prediction (rad/s) |
| `trajectory_spring_halflife` | 0.3 | Spring response time for trajectory smoothing |
| `force_search_threshold` | 2.0 | Input velocity delta that triggers an immediate search |
| `auto_input` | true | Read input actions automatically |

**Setup:**

1. Import a BVH file via the Kinetic dock (or script).
2. Assign the resulting MotionDatabase `.tres` resource to the player's `database` property.
3. Set `skeleton_path` to point at your character's Skeleton3D.
4. Ensure the Skeleton3D bone names match the BVH joint names.

**Input:** By default, reads `move_forward`, `move_back`, `move_left`, `move_right` input actions (falls back to `ui_up/down/left/right`). For camera-relative input, set `auto_input = false` and call `set_desired_velocity()` each frame.

### SpringUtils

Static utility class implementing critically-damped springs for inertialization. Based on Daniel Holden's (Ubisoft La Forge) halflife-parameterized spring formulations.

Provides:
- `decay_spring()` / `decay_spring_vec3()` -- decays an offset toward zero
- `decay_spring_quat()` -- quaternion rotation offset decay via scaled-axis representation
- `damper_spring_implicit()` / `damper_spring_implicit_vec3()` -- moves a value toward a goal
- `quat_to_scaled_axis()` / `scaled_axis_to_quat()` -- quaternion-axis conversions

## Inertialization

When the search selects a new frame that is not the natural successor of the current frame, a transition fires. At the moment of transition:

1. The offset between the current blended pose and the new animation pose is computed per bone (position and rotation).
2. This offset is stored and then exponentially decayed over subsequent frames using a critically-damped spring with the configured halflife.
3. Each frame, the final pose = animation pose + decaying offset.

This produces smooth, pop-free transitions without crossfading or blend trees.

## Current Limitations

- **Brute-force search.** Linear scan over all frames. Acceptable for databases up to ~50k frames on modern hardware. No acceleration structure (KD-tree, VP-tree) is implemented.
- **GDScript performance.** Feature computation and search are pure GDScript. Large databases (>100k frames) may require a native C++ port.
- **Single clip support.** The database can store multiple clip ranges, but the BVH importer produces one range per file. Multi-clip workflows require manual database merging.
- **No foot locking / IK.** Foot sliding is reduced by motion matching but not eliminated. Foot lock with IK correction is not implemented.
- **No animation events.** No support for triggering events (sounds, VFX) at specific frames.
- **Bone map rebuilt every frame.** The database-to-skeleton bone index mapping is recomputed each frame rather than cached.
- **BVH only.** No FBX, GLTF, or Godot Animation import path.
