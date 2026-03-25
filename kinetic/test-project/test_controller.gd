## Test controller that sets up a minimal skeleton and motion matching player.
##
## This script creates a simple skeleton at runtime, generates a synthetic
## MotionDatabase with basic walk/idle data, and wires up the
## MotionMatchingPlayer to drive it. Use WASD to move.
extends Node3D

var _skeleton: Skeleton3D
var _player: MotionMatchingPlayer
var _mesh: MeshInstance3D


func _ready() -> void:
	# --- Build a minimal test skeleton ---
	_skeleton = Skeleton3D.new()
	_skeleton.name = "Skeleton3D"
	add_child(_skeleton)

	# Simple skeleton: Hips -> Spine -> Head, Hips -> LeftFoot, Hips -> RightFoot
	var bone_names := ["Hips", "Spine", "Head", "LeftFoot", "RightFoot"]
	var bone_parents := [-1, 0, 1, 0, 0]
	var bone_rests := [
		Transform3D(Basis.IDENTITY, Vector3(0, 1, 0)),
		Transform3D(Basis.IDENTITY, Vector3(0, 0.4, 0)),
		Transform3D(Basis.IDENTITY, Vector3(0, 0.4, 0)),
		Transform3D(Basis.IDENTITY, Vector3(-0.15, -0.9, 0)),
		Transform3D(Basis.IDENTITY, Vector3(0.15, -0.9, 0)),
	]

	for i in range(bone_names.size()):
		var idx := _skeleton.add_bone(bone_names[i])
		if bone_parents[i] >= 0:
			_skeleton.set_bone_parent(idx, bone_parents[i])
		_skeleton.set_bone_rest(idx, bone_rests[i])

	# --- Add a capsule mesh for visibility ---
	_mesh = MeshInstance3D.new()
	var capsule := CapsuleMesh.new()
	capsule.radius = 0.2
	capsule.height = 1.8
	_mesh.mesh = capsule
	_mesh.position = Vector3(0, 0.9, 0)
	add_child(_mesh)

	# --- Build synthetic MotionDatabase ---
	var db := _build_synthetic_database()

	# --- Create MotionMatchingPlayer ---
	_player = MotionMatchingPlayer.new()
	_player.name = "MotionMatchingPlayer"
	_player.database = db
	_player.skeleton_path = _player.get_path_to(_skeleton)
	_player.auto_input = true
	add_child(_player)
	# The player needs the skeleton_path relative to itself
	_player.skeleton_path = _player.get_path_to(_skeleton)

	print("[Test] Kinetic motion matching test ready. Use WASD to move.")


## Build a synthetic motion database with simple procedural walk data.
## This produces enough data to test the search and inertialization systems.
func _build_synthetic_database() -> MotionDatabase:
	var db := MotionDatabase.new()
	var bone_count := 5  # Hips, Spine, Head, LeftFoot, RightFoot
	var fps := 60.0
	var clip_seconds := 4.0
	var frame_count := int(clip_seconds * fps)

	db.allocate(frame_count, bone_count)
	db.bone_parents = PackedInt32Array([-1, 0, 1, 0, 0])
	db.bone_names = PackedStringArray(["Hips", "Spine", "Head", "LeftFoot", "RightFoot"])
	db.left_foot_bone = 3
	db.right_foot_bone = 4
	db.hip_bone = 0
	db.transition_cost = 0.1

	# Generate a simple walking pattern
	var walk_speed := 1.5  # m/s
	var stride_length := 0.8
	var stride_freq := walk_speed / stride_length  # Hz

	for f in range(frame_count):
		var t := float(f) / fps
		var phase := t * stride_freq * TAU

		# Root moves forward along Z
		var root_pos := Vector3(0, 1.0, t * walk_speed)
		var root_dir := Vector3(0, 0, 1)

		db.set_root_position(f, root_pos)
		db.set_root_direction(f, root_dir)

		# Hips: slight vertical bob
		var hip_pos := Vector3(0, 1.0 + sin(phase * 2) * 0.02, t * walk_speed)
		db.set_bone_position(f, 0, hip_pos)
		db.set_bone_rotation(f, 0, Quaternion.IDENTITY)

		# Spine: local to hips
		db.set_bone_position(f, 1, Vector3(0, 0.4, 0))
		db.set_bone_rotation(f, 1, Quaternion.IDENTITY)

		# Head: local to spine
		db.set_bone_position(f, 2, Vector3(0, 0.4, 0))
		db.set_bone_rotation(f, 2, Quaternion.IDENTITY)

		# Left foot: oscillates forward/back with vertical lift
		var lf_phase := sin(phase)
		var lf_y := maxf(sin(phase), 0.0) * 0.1
		db.set_bone_position(f, 3, Vector3(-0.15, -0.9 + lf_y, lf_phase * 0.3))
		db.set_bone_rotation(f, 3, Quaternion.IDENTITY)

		# Right foot: opposite phase
		var rf_phase := sin(phase + PI)
		var rf_y := maxf(sin(phase + PI), 0.0) * 0.1
		db.set_bone_position(f, 4, Vector3(0.15, -0.9 + rf_y, rf_phase * 0.3))
		db.set_bone_rotation(f, 4, Quaternion.IDENTITY)

	# Compute velocities
	var frame_time := 1.0 / fps
	for f in range(frame_count):
		var prev := maxi(f - 1, 0)
		var nxt := mini(f + 1, frame_count - 1)
		var dt := frame_time * (2.0 if (f > 0 and f < frame_count - 1) else 1.0)
		for bone in range(bone_count):
			var p0 := db.get_bone_position(prev, bone)
			var p1 := db.get_bone_position(nxt, bone)
			db.set_bone_velocity(f, bone, (p1 - p0) / dt)
		var rp0 := db.get_root_position(prev)
		var rp1 := db.get_root_position(nxt)
		db.set_root_velocity(f, (rp1 - rp0) / dt)

	# Set clip range
	db.range_starts = PackedInt32Array([0])
	db.range_stops = PackedInt32Array([frame_count])

	# Build features
	db.build_features()

	print("[Test] Synthetic database: %d frames, %d bones, %d feature vectors." % [
		db.frame_count, db.bone_count, db.features.size() / MotionDatabase.FEATURE_DIM])

	return db
