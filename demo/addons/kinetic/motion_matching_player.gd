## Runtime motion matching player that drives a Skeleton3D node.
##
## Performs periodic search against a MotionDatabase, advances playback through
## the matched clip, and applies inertialized transitions when switching frames.
## Reads player input to build a desired trajectory for the query.
##
## Attach this node as a child (or sibling) of the character scene.
## Set skeleton_path to point at the Skeleton3D, and assign a built MotionDatabase.
class_name MotionMatchingPlayer
extends Node3D

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

## The motion database resource containing clips and precomputed features.
@export var database: MotionDatabase

## Path to the Skeleton3D node this player drives.
@export var skeleton_path: NodePath

## How often (in physics frames) to run the motion matching search.
## Default 6 corresponds to ~0.1s at 60fps.
@export_range(1, 30) var search_interval: int = 6

## Inertialization halflife in seconds. Controls how fast transition offsets decay.
@export_range(0.01, 1.0) var inertialization_halflife: float = 0.1

## Movement speed in meters per second for trajectory prediction.
@export_range(0.0, 10.0) var move_speed: float = 3.5

## Rotation speed in radians per second for trajectory prediction.
@export_range(0.0, 20.0) var turn_speed: float = 8.0

## Spring halflife for trajectory simulation (how quickly trajectory responds to input).
@export_range(0.01, 1.0) var trajectory_spring_halflife: float = 0.3

## Threshold for forced search on sharp input change (trajectory velocity delta).
@export_range(0.0, 5.0) var force_search_threshold: float = 2.0

## Whether to read input and update trajectory automatically. Disable for
## programmatic control via set_desired_velocity().
@export var auto_input: bool = true

# ---------------------------------------------------------------------------
# Runtime state
# ---------------------------------------------------------------------------

var _skeleton: Skeleton3D
var _current_frame: int = 0
var _frames_since_search: int = 0

# Inertialization offsets per bone
var _offset_positions: Array[Vector3] = []
var _offset_rotations: Array[Quaternion] = []
var _offset_vel_positions: Array[Vector3] = []
var _offset_vel_rotations: Array[Vector3] = []

# Simulation trajectory state
var _sim_position: Vector3 = Vector3.ZERO
var _sim_velocity: Vector3 = Vector3.ZERO
var _sim_direction: Vector3 = Vector3(0, 0, -1)
var _sim_rotation: float = 0.0  # yaw angle in radians

# Desired input
var _desired_velocity: Vector3 = Vector3.ZERO
var _prev_desired_velocity: Vector3 = Vector3.ZERO

# Trajectory prediction cache (root-local)
var _traj_positions: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]
var _traj_directions: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]


# ===========================================================================
# Lifecycle
# ===========================================================================

func _ready() -> void:
	if skeleton_path.is_empty():
		push_warning("MotionMatchingPlayer: skeleton_path is not set.")
		return

	_skeleton = get_node_or_null(skeleton_path) as Skeleton3D
	if not _skeleton:
		push_warning("MotionMatchingPlayer: Could not find Skeleton3D at path '%s'." % skeleton_path)
		return

	if not database:
		push_warning("MotionMatchingPlayer: No MotionDatabase assigned.")
		return

	if database.features.size() == 0:
		push_warning("MotionMatchingPlayer: Database has no feature vectors. Call build_features() first.")
		return

	# Initialize inertialization arrays
	_init_offsets()

	# Start at frame 0
	_current_frame = 0
	_sim_position = global_position
	_sim_direction = -global_basis.z


func _process(delta: float) -> void:
	if not _skeleton or not database or database.frame_count == 0:
		return

	# --- Read input ---
	if auto_input:
		_read_input()

	# --- Update simulation trajectory ---
	_update_simulation(delta)

	# --- Predict future trajectory ---
	_predict_trajectory(delta)

	# --- Search (periodic or forced) ---
	_frames_since_search += 1
	var force_search := false
	if _prev_desired_velocity.distance_to(_desired_velocity) > force_search_threshold:
		force_search = true
	_prev_desired_velocity = _desired_velocity

	if _frames_since_search >= search_interval or force_search:
		_run_search()
		_frames_since_search = 0

	# --- Advance playback ---
	_current_frame = clampi(_current_frame + 1, 0, database.frame_count - 1)

	# --- Apply pose with inertialization ---
	_apply_pose(delta)

	# --- Update node transform from simulation ---
	global_position = _sim_position


# ===========================================================================
# Input
# ===========================================================================

## Set desired movement velocity manually (for programmatic control).
## Disable auto_input when using this.
func set_desired_velocity(vel: Vector3) -> void:
	_desired_velocity = vel


## Read input from the default action map. Expects "move_left", "move_right",
## "move_forward", "move_back" input actions. Falls back to ui_* actions.
func _read_input() -> void:
	var input_vec := Vector2.ZERO

	if InputMap.has_action("move_forward"):
		input_vec.y -= Input.get_action_strength("move_forward")
		input_vec.y += Input.get_action_strength("move_back")
		input_vec.x -= Input.get_action_strength("move_left")
		input_vec.x += Input.get_action_strength("move_right")
	else:
		# Fallback to UI actions
		input_vec.y -= Input.get_action_strength("ui_up")
		input_vec.y += Input.get_action_strength("ui_down")
		input_vec.x -= Input.get_action_strength("ui_left")
		input_vec.x += Input.get_action_strength("ui_right")

	input_vec = input_vec.limit_length(1.0)

	# Convert to world-space velocity on XZ plane. The camera is assumed to
	# look along -Z (default Godot convention). For a third-person camera,
	# callers should use set_desired_velocity() with camera-relative input.
	_desired_velocity = Vector3(input_vec.x, 0.0, input_vec.y) * move_speed


# ===========================================================================
# Simulation & trajectory
# ===========================================================================

## Update the simulation object position/direction using spring-damped input.
func _update_simulation(delta: float) -> void:
	# Position: spring toward desired velocity
	var result := SpringUtils.damper_spring_implicit_vec3(
		_sim_position, _sim_velocity,
		_sim_position + _desired_velocity * delta * 10.0,
		trajectory_spring_halflife, delta)
	_sim_position = result[0]
	_sim_velocity = result[1]

	# Direction: rotate toward velocity direction
	if _desired_velocity.length_squared() > 0.01:
		var target_dir := _desired_velocity.normalized()
		target_dir.y = 0.0
		if target_dir.length_squared() > 0.001:
			_sim_direction = _sim_direction.lerp(target_dir.normalized(), clampf(turn_speed * delta, 0.0, 1.0))
			_sim_direction = _sim_direction.normalized()


## Predict future trajectory points at the standard offsets (+20, +40, +60 frames).
## Stores results in root-local space for feature matching.
func _predict_trajectory(_delta: float) -> void:
	var root_pos := _sim_position
	var root_rot := _direction_to_basis_quat(_sim_direction)
	var root_rot_inv := root_rot.inverse()

	# Simple linear extrapolation from current simulation state
	var dt_per_frame := 1.0 / 60.0
	for i in range(MotionDatabase.TRAJECTORY_POINTS):
		var t := float(MotionDatabase.TRAJECTORY_OFFSETS[i]) * dt_per_frame
		var future_pos := _sim_position + _sim_velocity * t
		var future_dir := _sim_direction

		# Store in root-local space
		_traj_positions[i] = root_rot_inv * (future_pos - root_pos)
		_traj_directions[i] = root_rot_inv * future_dir


# ===========================================================================
# Search
# ===========================================================================

## Build a query from the current state and search the database.
func _run_search() -> void:
	if not database or database.features.size() == 0:
		return

	# Get current pose features from the database for the current frame
	# (foot positions, foot velocities, hip velocity)
	var foot_l_pos := Vector3.ZERO
	var foot_r_pos := Vector3.ZERO
	var foot_l_vel := Vector3.ZERO
	var foot_r_vel := Vector3.ZERO
	var hip_vel := Vector3.ZERO

	if _current_frame >= 0 and _current_frame < database.frame_count:
		if database.left_foot_bone >= 0:
			foot_l_pos = database.compute_bone_root_space_position(_current_frame, database.left_foot_bone)
			foot_l_vel = database.compute_bone_root_space_velocity(_current_frame, database.left_foot_bone)
		if database.right_foot_bone >= 0:
			foot_r_pos = database.compute_bone_root_space_position(_current_frame, database.right_foot_bone)
			foot_r_vel = database.compute_bone_root_space_velocity(_current_frame, database.right_foot_bone)
		hip_vel = database.compute_bone_root_space_velocity(_current_frame, database.hip_bone)

	var best := database.query_match(
		foot_l_pos, foot_r_pos,
		foot_l_vel, foot_r_vel,
		hip_vel,
		_traj_positions, _traj_directions,
		_current_frame
	)

	if best >= 0 and best != _current_frame + 1:
		_transition_to(best)


# ===========================================================================
# Inertialization
# ===========================================================================

## Initialize inertialization offset arrays.
func _init_offsets() -> void:
	var bc := database.bone_count
	_offset_positions.resize(bc)
	_offset_rotations.resize(bc)
	_offset_vel_positions.resize(bc)
	_offset_vel_rotations.resize(bc)
	for i in range(bc):
		_offset_positions[i] = Vector3.ZERO
		_offset_rotations[i] = Quaternion.IDENTITY
		_offset_vel_positions[i] = Vector3.ZERO
		_offset_vel_rotations[i] = Vector3.ZERO


## Trigger a transition from the current frame to a new target frame.
## Computes inertialization offsets at the moment of transition.
func _transition_to(new_frame: int) -> void:
	var old_frame := _current_frame

	for bone in range(database.bone_count):
		# Source pose = old animation + current offset
		var src_pos := database.get_bone_position(old_frame, bone) + _offset_positions[bone]
		var src_rot := database.get_bone_rotation(old_frame, bone) * _offset_rotations[bone]
		var src_vel := database.get_bone_velocity(old_frame, bone) + _offset_vel_positions[bone]

		# Destination pose
		var dst_pos := database.get_bone_position(new_frame, bone)
		var dst_rot := database.get_bone_rotation(new_frame, bone)
		var dst_vel := database.get_bone_velocity(new_frame, bone)

		# New offset = source - destination
		_offset_positions[bone] = src_pos - dst_pos
		_offset_vel_positions[bone] = src_vel - dst_vel

		# Rotation offset: src * dst.inverse()
		_offset_rotations[bone] = src_rot * dst_rot.inverse()
		# Ensure shortest path
		if _offset_rotations[bone].w < 0.0:
			_offset_rotations[bone] = Quaternion(
				-_offset_rotations[bone].x, -_offset_rotations[bone].y,
				-_offset_rotations[bone].z, -_offset_rotations[bone].w)

		_offset_vel_rotations[bone] = Vector3.ZERO

	_current_frame = new_frame


# ===========================================================================
# Pose application
# ===========================================================================

## Apply the current database frame to the Skeleton3D, with inertialization decay.
func _apply_pose(delta: float) -> void:
	if _current_frame < 0 or _current_frame >= database.frame_count:
		return

	var bone_map := _build_bone_map()

	for bone in range(database.bone_count):
		# Decay inertialization offsets
		var spring_pos := SpringUtils.decay_spring_vec3(
			_offset_positions[bone], _offset_vel_positions[bone],
			inertialization_halflife, delta)
		_offset_positions[bone] = spring_pos[0]
		_offset_vel_positions[bone] = spring_pos[1]

		var spring_rot := SpringUtils.decay_spring_quat(
			_offset_rotations[bone], _offset_vel_rotations[bone],
			inertialization_halflife, delta)
		_offset_rotations[bone] = spring_rot[0]
		_offset_vel_rotations[bone] = spring_rot[1]

		# Final pose = animation pose + decaying offset
		var anim_pos := database.get_bone_position(_current_frame, bone)
		var anim_rot := database.get_bone_rotation(_current_frame, bone)

		var final_pos := anim_pos + _offset_positions[bone]
		var final_rot := (_offset_rotations[bone] * anim_rot).normalized()

		# Apply to skeleton
		var skel_bone_idx: int = bone_map.get(bone, -1)
		if skel_bone_idx >= 0:
			_skeleton.set_bone_pose_position(skel_bone_idx, final_pos)
			_skeleton.set_bone_pose_rotation(skel_bone_idx, final_rot)


## Build a mapping from database bone index to skeleton bone index by name.
## Cached — this is called every frame but the result could be stored.
## For a prototype this is acceptable.
func _build_bone_map() -> Dictionary:
	var map := {}
	if not _skeleton or database.bone_names.size() == 0:
		# Fallback: identity mapping (database bone i = skeleton bone i)
		for i in range(database.bone_count):
			if i < _skeleton.get_bone_count():
				map[i] = i
		return map

	for i in range(database.bone_names.size()):
		var name_str: String = database.bone_names[i]
		var idx := _skeleton.find_bone(name_str)
		if idx >= 0:
			map[i] = idx
	return map


## Convert a direction vector to a quaternion rotation (Y-up, facing direction).
func _direction_to_basis_quat(dir: Vector3) -> Quaternion:
	if dir.length_squared() < 1e-8:
		return Quaternion.IDENTITY
	var forward := dir.normalized()
	forward.y = 0.0
	if forward.length_squared() < 1e-8:
		return Quaternion.IDENTITY
	forward = forward.normalized()
	var basis := Basis.looking_at(forward, Vector3.UP)
	return basis.get_rotation_quaternion()
