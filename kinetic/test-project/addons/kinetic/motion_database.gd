## Motion database resource for motion matching.
##
## Stores motion clips as flat arrays of pose frames. Each frame contains per-bone
## transforms and trajectory data. Feature vectors (27 floats) are computed for
## efficient nearest-neighbor search during runtime.
##
## Feature vector layout (27 floats):
##   [0..2]   Left foot position (root space)
##   [3..5]   Right foot position (root space)
##   [6..8]   Left foot velocity (root space)
##   [9..11]  Right foot velocity (root space)
##   [12..14] Hip velocity (root space)
##   [15..20] Trajectory positions at +20/+40/+60 frames (xz pairs)
##   [21..26] Trajectory directions at +20/+40/+60 frames (xz pairs)
@tool
class_name MotionDatabase
extends Resource

## Number of floats in each feature vector.
const FEATURE_DIM := 27

## Number of future trajectory sample points.
const TRAJECTORY_POINTS := 3

## Frame offsets for trajectory samples (at 60fps: +0.33s, +0.67s, +1.0s).
const TRAJECTORY_OFFSETS: Array[int] = [20, 40, 60]

## Frames to exclude at the end of each range to avoid matching near clip boundaries.
const IGNORE_RANGE_END := 20

## Frames around the current frame to skip during search (hysteresis window).
const IGNORE_SURROUNDING := 20

## Default feature weights for search. Higher weight = more influence on matching.
static var DEFAULT_WEIGHTS: PackedFloat32Array = PackedFloat32Array([
	0.75, 0.75, 0.75,   # left foot pos
	0.75, 0.75, 0.75,   # right foot pos
	1.0, 1.0, 1.0,      # left foot vel
	1.0, 1.0, 1.0,      # right foot vel
	1.0, 1.0, 1.0,      # hip vel
	1.0, 1.0,            # traj pos +20 (xz)
	1.0, 1.0,            # traj pos +40 (xz)
	1.0, 1.0,            # traj pos +60 (xz)
	1.5, 1.5,            # traj dir +20 (xz)
	1.5, 1.5,            # traj dir +40 (xz)
	1.5, 1.5,            # traj dir +60 (xz)
])

# ---------------------------------------------------------------------------
# Exported data
# ---------------------------------------------------------------------------

## Total number of frames across all clips.
@export var frame_count: int = 0

## Number of bones per frame.
@export var bone_count: int = 0

## Bone parent indices. Index -1 means root bone (no parent).
@export var bone_parents: PackedInt32Array = PackedInt32Array()

## Bone names, parallel to bone_parents.
@export var bone_names: PackedStringArray = PackedStringArray()

## Index of the left foot bone (typically "LeftFoot" or "LeftToe").
@export var left_foot_bone: int = -1

## Index of the right foot bone (typically "RightFoot" or "RightToe").
@export var right_foot_bone: int = -1

## Index of the hip/root bone for velocity features.
@export var hip_bone: int = 0

## Clip range starts. Each range is a contiguous block of frames from one clip.
@export var range_starts: PackedInt32Array = PackedInt32Array()

## Clip range ends (exclusive).
@export var range_stops: PackedInt32Array = PackedInt32Array()

## Transition cost penalty — current frame must be beaten by at least this margin.
@export var transition_cost: float = 0.0

## Feature weights for weighted Euclidean distance. Length must equal FEATURE_DIM.
@export var feature_weights: PackedFloat32Array = DEFAULT_WEIGHTS.duplicate()

# ---------------------------------------------------------------------------
# Per-frame bone data — flat arrays, indexed as [frame * bone_count + bone]
# ---------------------------------------------------------------------------

## Bone local positions packed as sequential Vector3 values (x,y,z triples).
## Total length: frame_count * bone_count * 3.
@export var bone_positions: PackedFloat32Array = PackedFloat32Array()

## Bone local rotations packed as sequential quaternions (x,y,z,w quads).
## Total length: frame_count * bone_count * 4.
@export var bone_rotations: PackedFloat32Array = PackedFloat32Array()

## Bone local velocities packed as Vector3 (x,y,z triples).
## Total length: frame_count * bone_count * 3.
@export var bone_velocities: PackedFloat32Array = PackedFloat32Array()

## Bone local angular velocities packed as Vector3 (x,y,z triples).
## Total length: frame_count * bone_count * 3.
@export var bone_angular_velocities: PackedFloat32Array = PackedFloat32Array()

# ---------------------------------------------------------------------------
# Root (simulation bone) trajectory for each frame
# ---------------------------------------------------------------------------

## Root world positions per frame (x,y,z triples). Length: frame_count * 3.
@export var root_positions: PackedFloat32Array = PackedFloat32Array()

## Root world forward directions per frame (x,y,z triples). Length: frame_count * 3.
@export var root_directions: PackedFloat32Array = PackedFloat32Array()

## Root world velocities per frame (x,y,z triples). Length: frame_count * 3.
@export var root_velocities: PackedFloat32Array = PackedFloat32Array()

# ---------------------------------------------------------------------------
# Feature vectors for search
# ---------------------------------------------------------------------------

## Precomputed feature vectors. Length: frame_count * FEATURE_DIM.
@export var features: PackedFloat32Array = PackedFloat32Array()

## Per-feature mean (for normalization). Length: FEATURE_DIM.
@export var feature_offset: PackedFloat32Array = PackedFloat32Array()

## Per-feature scale (for normalization). Length: FEATURE_DIM.
@export var feature_scale: PackedFloat32Array = PackedFloat32Array()


# ===========================================================================
# Bone data accessors
# ===========================================================================

## Get bone position for the given frame and bone index.
func get_bone_position(frame: int, bone: int) -> Vector3:
	var idx := (frame * bone_count + bone) * 3
	return Vector3(bone_positions[idx], bone_positions[idx + 1], bone_positions[idx + 2])


## Get bone rotation for the given frame and bone index.
func get_bone_rotation(frame: int, bone: int) -> Quaternion:
	var idx := (frame * bone_count + bone) * 4
	return Quaternion(bone_rotations[idx], bone_rotations[idx + 1],
		bone_rotations[idx + 2], bone_rotations[idx + 3])


## Get bone velocity for the given frame and bone index.
func get_bone_velocity(frame: int, bone: int) -> Vector3:
	var idx := (frame * bone_count + bone) * 3
	return Vector3(bone_velocities[idx], bone_velocities[idx + 1], bone_velocities[idx + 2])


## Get bone angular velocity for the given frame and bone index.
func get_bone_angular_velocity(frame: int, bone: int) -> Vector3:
	var idx := (frame * bone_count + bone) * 3
	return Vector3(bone_angular_velocities[idx], bone_angular_velocities[idx + 1],
		bone_angular_velocities[idx + 2])


## Get root position at a given frame.
func get_root_position(frame: int) -> Vector3:
	var idx := frame * 3
	return Vector3(root_positions[idx], root_positions[idx + 1], root_positions[idx + 2])


## Get root forward direction at a given frame.
func get_root_direction(frame: int) -> Vector3:
	var idx := frame * 3
	return Vector3(root_directions[idx], root_directions[idx + 1], root_directions[idx + 2])


## Get root velocity at a given frame.
func get_root_velocity(frame: int) -> Vector3:
	var idx := frame * 3
	return Vector3(root_velocities[idx], root_velocities[idx + 1], root_velocities[idx + 2])


## Set bone position for the given frame and bone index.
func set_bone_position(frame: int, bone: int, pos: Vector3) -> void:
	var idx := (frame * bone_count + bone) * 3
	bone_positions[idx] = pos.x
	bone_positions[idx + 1] = pos.y
	bone_positions[idx + 2] = pos.z


## Set bone rotation for the given frame and bone index.
func set_bone_rotation(frame: int, bone: int, rot: Quaternion) -> void:
	var idx := (frame * bone_count + bone) * 4
	bone_rotations[idx] = rot.x
	bone_rotations[idx + 1] = rot.y
	bone_rotations[idx + 2] = rot.z
	bone_rotations[idx + 3] = rot.w


## Set bone velocity for the given frame and bone index.
func set_bone_velocity(frame: int, bone: int, vel: Vector3) -> void:
	var idx := (frame * bone_count + bone) * 3
	bone_velocities[idx] = vel.x
	bone_velocities[idx + 1] = vel.y
	bone_velocities[idx + 2] = vel.z


## Set bone angular velocity for the given frame and bone index.
func set_bone_angular_velocity(frame: int, bone: int, ang_vel: Vector3) -> void:
	var idx := (frame * bone_count + bone) * 3
	bone_angular_velocities[idx] = ang_vel.x
	bone_angular_velocities[idx + 1] = ang_vel.y
	bone_angular_velocities[idx + 2] = ang_vel.z


## Set root position at a given frame.
func set_root_position(frame: int, pos: Vector3) -> void:
	var idx := frame * 3
	root_positions[idx] = pos.x
	root_positions[idx + 1] = pos.y
	root_positions[idx + 2] = pos.z


## Set root forward direction at a given frame.
func set_root_direction(frame: int, dir: Vector3) -> void:
	var idx := frame * 3
	root_directions[idx] = dir.x
	root_directions[idx + 1] = dir.y
	root_directions[idx + 2] = dir.z


## Set root velocity at a given frame.
func set_root_velocity(frame: int, vel: Vector3) -> void:
	var idx := frame * 3
	root_velocities[idx] = vel.x
	root_velocities[idx + 1] = vel.y
	root_velocities[idx + 2] = vel.z


# ===========================================================================
# Allocation
# ===========================================================================

## Allocate all internal arrays for the given frame_count and bone_count.
## Must be called before populating data.
func allocate(p_frame_count: int, p_bone_count: int) -> void:
	frame_count = p_frame_count
	bone_count = p_bone_count

	bone_positions.resize(frame_count * bone_count * 3)
	bone_positions.fill(0.0)
	bone_rotations.resize(frame_count * bone_count * 4)
	bone_rotations.fill(0.0)
	# Default rotations to identity quaternion (0,0,0,1)
	for i in range(frame_count * bone_count):
		bone_rotations[i * 4 + 3] = 1.0

	bone_velocities.resize(frame_count * bone_count * 3)
	bone_velocities.fill(0.0)
	bone_angular_velocities.resize(frame_count * bone_count * 3)
	bone_angular_velocities.fill(0.0)

	root_positions.resize(frame_count * 3)
	root_positions.fill(0.0)
	root_directions.resize(frame_count * 3)
	root_directions.fill(0.0)
	root_velocities.resize(frame_count * 3)
	root_velocities.fill(0.0)

	features.resize(0)
	feature_offset.resize(0)
	feature_scale.resize(0)


# ===========================================================================
# Forward kinematics
# ===========================================================================

## Compute the world-space position and rotation of a bone at a given frame
## by walking up the parent chain. Returns [world_position, world_rotation].
func compute_bone_world_transform(frame: int, bone: int) -> Array:
	var pos := get_bone_position(frame, bone)
	var rot := get_bone_rotation(frame, bone)
	var parent := bone_parents[bone] if bone < bone_parents.size() else -1
	while parent >= 0:
		var parent_pos := get_bone_position(frame, parent)
		var parent_rot := get_bone_rotation(frame, parent)
		pos = parent_pos + parent_rot * pos
		rot = parent_rot * rot
		parent = bone_parents[parent] if parent < bone_parents.size() else -1
	return [pos, rot]


## Compute a bone position in root-local space (relative to bone 0).
## This is what the feature vector expects.
func compute_bone_root_space_position(frame: int, bone: int) -> Vector3:
	var world := compute_bone_world_transform(frame, bone)
	var root_pos := get_bone_position(frame, 0)
	var root_rot := get_bone_rotation(frame, 0)
	var root_rot_inv := root_rot.inverse()
	return root_rot_inv * (world[0] as Vector3 - root_pos)


## Compute a bone velocity in root-local space.
func compute_bone_root_space_velocity(frame: int, bone: int) -> Vector3:
	# Approximate via finite difference of world positions if velocities are not
	# precomputed in world space. For efficiency, we use the stored local velocity
	# transformed by the parent chain's rotation.
	var vel := get_bone_velocity(frame, bone)
	var parent := bone_parents[bone] if bone < bone_parents.size() else -1
	while parent >= 0:
		var parent_rot := get_bone_rotation(frame, parent)
		vel = parent_rot * vel
		parent = bone_parents[parent] if parent < bone_parents.size() else -1
	# Transform into root space
	var root_rot := get_bone_rotation(frame, 0)
	return root_rot.inverse() * vel


# ===========================================================================
# Feature computation
# ===========================================================================

## Build feature vectors for all frames. Call this after populating all bone
## and root data. Features are normalized using z-score normalization with
## per-group weighting.
func build_features() -> void:
	if frame_count == 0 or bone_count == 0:
		push_warning("MotionDatabase: Cannot build features — no data loaded.")
		return

	if left_foot_bone < 0 or right_foot_bone < 0:
		push_warning("MotionDatabase: Left/right foot bone indices not set. Feature quality will be degraded.")

	features.resize(frame_count * FEATURE_DIM)
	features.fill(0.0)

	# --- Compute raw features for every frame ---
	for f in range(frame_count):
		var base := f * FEATURE_DIM

		# Left foot position (root space) [0..2]
		if left_foot_bone >= 0:
			var lfp := compute_bone_root_space_position(f, left_foot_bone)
			features[base + 0] = lfp.x
			features[base + 1] = lfp.y
			features[base + 2] = lfp.z

		# Right foot position (root space) [3..5]
		if right_foot_bone >= 0:
			var rfp := compute_bone_root_space_position(f, right_foot_bone)
			features[base + 3] = rfp.x
			features[base + 4] = rfp.y
			features[base + 5] = rfp.z

		# Left foot velocity (root space) [6..8]
		if left_foot_bone >= 0:
			var lfv := compute_bone_root_space_velocity(f, left_foot_bone)
			features[base + 6] = lfv.x
			features[base + 7] = lfv.y
			features[base + 8] = lfv.z

		# Right foot velocity (root space) [9..11]
		if right_foot_bone >= 0:
			var rfv := compute_bone_root_space_velocity(f, right_foot_bone)
			features[base + 9] = rfv.x
			features[base + 10] = rfv.y
			features[base + 11] = rfv.z

		# Hip velocity (root space) [12..14]
		var hv := compute_bone_root_space_velocity(f, hip_bone)
		features[base + 12] = hv.x
		features[base + 13] = hv.y
		features[base + 14] = hv.z

		# Trajectory positions (xz) at future offsets [15..20]
		var root_pos := get_root_position(f)
		var root_dir := get_root_direction(f)
		var root_rot := _direction_to_rotation(root_dir)
		var root_rot_inv := root_rot.inverse()

		for ti in range(TRAJECTORY_POINTS):
			var future_frame := clampi(f + TRAJECTORY_OFFSETS[ti], 0, frame_count - 1)
			var future_pos := get_root_position(future_frame)
			var local_pos := root_rot_inv * (future_pos - root_pos)
			features[base + 15 + ti * 2 + 0] = local_pos.x
			features[base + 15 + ti * 2 + 1] = local_pos.z

		# Trajectory directions (xz) at future offsets [21..26]
		for ti in range(TRAJECTORY_POINTS):
			var future_frame := clampi(f + TRAJECTORY_OFFSETS[ti], 0, frame_count - 1)
			var future_dir := get_root_direction(future_frame)
			var local_dir := root_rot_inv * future_dir
			features[base + 21 + ti * 2 + 0] = local_dir.x
			features[base + 21 + ti * 2 + 1] = local_dir.z

	# --- Normalize features ---
	_normalize_features()


## Internal: Normalize the feature array using z-score normalization with weighting.
## Stores offset (mean) and scale (std/weight) per dimension.
func _normalize_features() -> void:
	feature_offset.resize(FEATURE_DIM)
	feature_scale.resize(FEATURE_DIM)
	feature_offset.fill(0.0)
	feature_scale.fill(1.0)

	if frame_count == 0:
		return

	# Compute per-dimension mean
	for d in range(FEATURE_DIM):
		var sum := 0.0
		for f in range(frame_count):
			sum += features[f * FEATURE_DIM + d]
		feature_offset[d] = sum / float(frame_count)

	# Compute per-dimension standard deviation
	for d in range(FEATURE_DIM):
		var sum_sq := 0.0
		var mean := feature_offset[d]
		for f in range(frame_count):
			var diff := features[f * FEATURE_DIM + d] - mean
			sum_sq += diff * diff
		var std := sqrt(sum_sq / float(frame_count))
		# Scale = std / weight. If std is near zero, use 1.0 to avoid division issues.
		var w := feature_weights[d] if d < feature_weights.size() else 1.0
		feature_scale[d] = maxf(std, 1e-5) / maxf(w, 1e-5)

	# Apply normalization in-place: (value - mean) / scale
	for f in range(frame_count):
		for d in range(FEATURE_DIM):
			var idx := f * FEATURE_DIM + d
			features[idx] = (features[idx] - feature_offset[d]) / feature_scale[d]


## Helper: Construct a basis rotation from a forward direction vector (Y-up).
func _direction_to_rotation(dir: Vector3) -> Quaternion:
	if dir.length_squared() < 1e-8:
		return Quaternion.IDENTITY
	var forward := dir.normalized()
	# Project to XZ plane for horizontal direction
	forward.y = 0.0
	if forward.length_squared() < 1e-8:
		return Quaternion.IDENTITY
	forward = forward.normalized()
	# Godot uses -Z as forward in its convention, but we follow the reference
	# where the simulation bone's forward is the stored direction.
	var basis := Basis.looking_at(forward, Vector3.UP)
	return basis.get_rotation_quaternion()


# ===========================================================================
# Search
# ===========================================================================

## Normalize a raw query feature vector using the database's stored offset/scale.
## The input should be 27 raw floats in the same layout as the features.
func normalize_query(raw_query: PackedFloat32Array) -> PackedFloat32Array:
	var normalized := PackedFloat32Array()
	normalized.resize(FEATURE_DIM)
	for d in range(FEATURE_DIM):
		normalized[d] = (raw_query[d] - feature_offset[d]) / feature_scale[d]
	return normalized


## Brute-force linear scan over all frames, returning the index of the best
## matching frame. Uses weighted Euclidean distance with early-out.
##
## Parameters:
##   query          — normalized feature vector (FEATURE_DIM floats)
##   current_frame  — the frame currently being played (-1 if none)
##
## Returns the frame index of the best match, or -1 if the database is empty.
func search(query: PackedFloat32Array, current_frame: int = -1) -> int:
	if features.size() == 0 or frame_count == 0:
		return -1

	var best_cost := INF
	var best_frame := -1
	var n_ranges := range_starts.size()

	# If no ranges defined, treat the whole database as one range
	if n_ranges == 0:
		best_frame = _scan_range(query, 0, frame_count, current_frame, best_cost)
		return best_frame

	for r in range(n_ranges):
		var rstart: int = range_starts[r]
		var rstop: int = range_stops[r]
		# Exclude last IGNORE_RANGE_END frames from this range
		var search_end := maxi(rstart, rstop - IGNORE_RANGE_END)

		# Pass current best_cost so _scan_range can early-out against it.
		# After finding a candidate, recompute its exact cost to update best_cost
		# for subsequent ranges.
		var candidate := _scan_range(query, rstart, search_end, current_frame, best_cost)
		if candidate >= 0:
			var cost := _compute_cost(query, candidate, current_frame)
			if cost < best_cost:
				best_cost = cost
				best_frame = candidate

	return best_frame


## Internal: Scan a range of frames and return the best matching frame index.
## Updates best_cost by reference through return value pattern.
func _scan_range(query: PackedFloat32Array, start: int, end: int,
		current_frame: int, best_cost_in: float) -> int:
	var best_cost := best_cost_in
	var best_frame := -1

	for f in range(start, end):
		# Skip frames near the current playback position
		if current_frame >= 0 and absi(f - current_frame) < IGNORE_SURROUNDING:
			continue

		# Weighted L2 squared distance with early-out
		var cost := 0.0
		var base := f * FEATURE_DIM
		var broke_early := false

		for d in range(FEATURE_DIM):
			var diff := query[d] - features[base + d]
			cost += diff * diff
			# Early-out: if partial cost already exceeds best, skip this frame
			if cost >= best_cost:
				broke_early = true
				break

		if broke_early:
			continue

		# Add transition cost if we are switching away from the current frame
		if current_frame >= 0 and f != current_frame + 1:
			cost += transition_cost

		if cost < best_cost:
			best_cost = cost
			best_frame = f

	return best_frame


## Internal: Compute the full cost for a specific frame (used for final comparison
## across ranges after _scan_range identifies candidates).
func _compute_cost(query: PackedFloat32Array, frame: int, current_frame: int) -> float:
	var cost := 0.0
	var base := frame * FEATURE_DIM
	for d in range(FEATURE_DIM):
		var diff := query[d] - features[base + d]
		cost += diff * diff
	if current_frame >= 0 and frame != current_frame + 1:
		cost += transition_cost
	return cost


## Convenience: Construct a query feature vector from game state and search.
## This is the primary entry point for runtime use.
##
## Parameters:
##   foot_l_pos    — left foot position in root space
##   foot_r_pos    — right foot position in root space
##   foot_l_vel    — left foot velocity in root space
##   foot_r_vel    — right foot velocity in root space
##   hip_vel       — hip velocity in root space
##   traj_pos      — array of 3 future trajectory positions (Vector3, root-local)
##   traj_dir      — array of 3 future trajectory directions (Vector3, root-local)
##   current_frame — current playback frame
##
## Returns the best matching frame index.
func query_match(
	foot_l_pos: Vector3, foot_r_pos: Vector3,
	foot_l_vel: Vector3, foot_r_vel: Vector3,
	hip_vel: Vector3,
	traj_pos: Array[Vector3], traj_dir: Array[Vector3],
	current_frame: int = -1
) -> int:
	var raw := PackedFloat32Array()
	raw.resize(FEATURE_DIM)

	raw[0] = foot_l_pos.x; raw[1] = foot_l_pos.y; raw[2] = foot_l_pos.z
	raw[3] = foot_r_pos.x; raw[4] = foot_r_pos.y; raw[5] = foot_r_pos.z
	raw[6] = foot_l_vel.x; raw[7] = foot_l_vel.y; raw[8] = foot_l_vel.z
	raw[9] = foot_r_vel.x; raw[10] = foot_r_vel.y; raw[11] = foot_r_vel.z
	raw[12] = hip_vel.x; raw[13] = hip_vel.y; raw[14] = hip_vel.z

	for i in range(mini(TRAJECTORY_POINTS, traj_pos.size())):
		raw[15 + i * 2 + 0] = traj_pos[i].x
		raw[15 + i * 2 + 1] = traj_pos[i].z

	for i in range(mini(TRAJECTORY_POINTS, traj_dir.size())):
		raw[21 + i * 2 + 0] = traj_dir[i].x
		raw[21 + i * 2 + 1] = traj_dir[i].z

	var normalized := normalize_query(raw)
	return search(normalized, current_frame)
