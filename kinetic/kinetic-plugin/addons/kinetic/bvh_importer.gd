## BVH (Biovision Hierarchy) file importer.
##
## Reads a standard BVH file and produces a MotionDatabase resource populated
## with bone hierarchy, per-frame transforms, root trajectory, and precomputed
## feature vectors ready for motion matching search.
##
## Usage:
##   var importer := BVHImporter.new()
##   var db := importer.import_file("res://mocap/walk.bvh")
##   if db:
##       db.build_features()
##       ResourceSaver.save(db, "res://mocap/walk.tres")
##
## Supports:
##   - HIERARCHY section with JOINT, End Site, OFFSET, CHANNELS
##   - MOTION section with frame count, frame time, and channel data
##   - Channel orders: Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation
##   - Arbitrary channel subsets per joint (3 or 6 channels)
##
## Limitations:
##   - Assumes degrees for rotation channels (standard BVH)
##   - Does not handle CHANNEL orders other than position XYZ / rotation XYZ/ZYX/etc.
##   - Velocities are computed via central finite differences after loading
class_name BVHImporter
extends RefCounted

# ---------------------------------------------------------------------------
# Internal structures
# ---------------------------------------------------------------------------

## Parsed joint info from the HIERARCHY section.
class BVHJoint:
	var name: String = ""
	var parent_index: int = -1
	var offset: Vector3 = Vector3.ZERO
	var channels: PackedStringArray = PackedStringArray()
	var channel_start: int = 0  # index into the flat channel data row
	var is_end_site: bool = false

# ---------------------------------------------------------------------------
# Importer state
# ---------------------------------------------------------------------------

var _joints: Array[BVHJoint] = []
var _total_channels: int = 0

## Left foot joint name patterns to auto-detect.
var left_foot_names: PackedStringArray = PackedStringArray([
	"LeftFoot", "leftfoot", "Left_Foot", "lfoot", "LFoot",
	"LeftToeBase", "LeftToe", "Left_Toe",
])

## Right foot joint name patterns to auto-detect.
var right_foot_names: PackedStringArray = PackedStringArray([
	"RightFoot", "rightfoot", "Right_Foot", "rfoot", "RFoot",
	"RightToeBase", "RightToe", "Right_Toe",
])

## Hip joint name patterns to auto-detect.
var hip_names: PackedStringArray = PackedStringArray([
	"Hips", "hips", "Hip", "hip", "Pelvis", "pelvis", "Root", "root",
])


# ===========================================================================
# Public API
# ===========================================================================

## Import a BVH file from disk and return a populated MotionDatabase.
## Returns null on failure. The database will have features built automatically.
##
## Parameters:
##   path                  — file path (absolute or res://)
##   auto_build_features   — if true, calls build_features() before returning
##   compute_velocities    — if true, computes velocities via finite differences
func import_file(path: String, auto_build_features: bool = true,
		compute_velocities: bool = true) -> MotionDatabase:
	var file := FileAccess.open(path, FileAccess.READ)
	if not file:
		push_error("BVHImporter: Could not open file '%s'." % path)
		return null

	var text := file.get_as_text()
	file.close()
	return import_text(text, auto_build_features, compute_velocities)


## Import BVH data from a string.
func import_text(text: String, auto_build_features: bool = true,
		compute_velocities: bool = true) -> MotionDatabase:
	_joints.clear()
	_total_channels = 0

	var lines := text.split("\n")
	var line_idx := 0

	# --- Parse HIERARCHY ---
	line_idx = _parse_hierarchy(lines, line_idx)
	if _joints.size() == 0:
		push_error("BVHImporter: No joints found in hierarchy.")
		return null

	# --- Parse MOTION ---
	var motion_result := _parse_motion(lines, line_idx)
	if motion_result == null:
		return null

	var frame_count: int = motion_result["frame_count"]
	var frame_time: float = motion_result["frame_time"]
	var channel_data: Array = motion_result["data"]  # Array of PackedFloat64Array

	# --- Build MotionDatabase ---
	var db := MotionDatabase.new()

	# Count non-end-site joints for bone data
	var real_joints: Array[BVHJoint] = []
	for j in _joints:
		if not j.is_end_site:
			real_joints.append(j)

	var bone_count := real_joints.size()
	db.allocate(frame_count, bone_count)

	# Bone parents and names
	db.bone_parents.resize(bone_count)
	db.bone_names.resize(bone_count)

	# Build index map: original joint index -> real bone index
	var joint_to_bone: Dictionary = {}
	var bi := 0
	for ji in range(_joints.size()):
		if not _joints[ji].is_end_site:
			joint_to_bone[ji] = bi
			bi += 1

	for ji in range(_joints.size()):
		if _joints[ji].is_end_site:
			continue
		var bone_idx: int = joint_to_bone[ji]
		db.bone_names[bone_idx] = _joints[ji].name
		if _joints[ji].parent_index >= 0 and joint_to_bone.has(_joints[ji].parent_index):
			db.bone_parents[bone_idx] = joint_to_bone[_joints[ji].parent_index]
		else:
			db.bone_parents[bone_idx] = -1

	# Auto-detect foot and hip bones
	db.left_foot_bone = _find_bone_by_name(db.bone_names, left_foot_names)
	db.right_foot_bone = _find_bone_by_name(db.bone_names, right_foot_names)
	db.hip_bone = _find_bone_by_name(db.bone_names, hip_names)
	if db.hip_bone < 0:
		db.hip_bone = 0  # fallback to root

	# --- Populate per-frame data ---
	for f in range(frame_count):
		var row: PackedFloat64Array = channel_data[f]

		for ji in range(_joints.size()):
			var joint := _joints[ji]
			if joint.is_end_site:
				continue

			var bone_idx: int = joint_to_bone[ji]
			var pos := joint.offset
			var rot := Quaternion.IDENTITY

			# Read channels
			var ch_start := joint.channel_start
			var rot_order: Array[String] = []

			for ci in range(joint.channels.size()):
				var ch_name: String = joint.channels[ci]
				var val: float = row[ch_start + ci] if (ch_start + ci) < row.size() else 0.0

				match ch_name:
					"Xposition":
						pos.x = val
					"Yposition":
						pos.y = val
					"Zposition":
						pos.z = val
					"Xrotation", "Yrotation", "Zrotation":
						rot_order.append(ch_name)

			# Build rotation from Euler channels in the order they appear
			if rot_order.size() > 0:
				rot = _build_rotation(joint, row, ch_start, rot_order)

			# Root bone (index 0): treat position as world position.
			# Non-root bones: position is the joint offset (local to parent).
			# BVH root has position channels that override the offset.
			if bone_idx == 0:
				db.set_bone_position(f, bone_idx, pos)
				db.set_root_position(f, pos)
				# Root direction from rotation (forward = -Z in Godot)
				var fwd := rot * Vector3(0, 0, 1)
				fwd.y = 0.0
				if fwd.length_squared() > 1e-8:
					fwd = fwd.normalized()
				else:
					fwd = Vector3(0, 0, 1)
				db.set_root_direction(f, fwd)
			else:
				# For non-root joints in BVH, the offset is the rest position.
				# If the joint has position channels, those override it.
				# Most BVH files only give position channels to the root.
				var has_pos_channels := false
				for ch in joint.channels:
					if ch.ends_with("position"):
						has_pos_channels = true
						break
				if not has_pos_channels:
					pos = joint.offset
				db.set_bone_position(f, bone_idx, pos)

			db.set_bone_rotation(f, bone_idx, rot)

	# --- Compute velocities via central finite differences ---
	if compute_velocities:
		_compute_velocities(db, frame_time)

	# --- Define range covering the entire clip ---
	db.range_starts = PackedInt32Array([0])
	db.range_stops = PackedInt32Array([frame_count])

	# --- Build features ---
	if auto_build_features:
		db.build_features()

	return db


# ===========================================================================
# Hierarchy parsing
# ===========================================================================

## Parse the HIERARCHY section. Returns the line index after parsing.
func _parse_hierarchy(lines: PackedStringArray, start: int) -> int:
	var idx := start
	var parent_stack: Array[int] = []  # stack of joint indices
	var current_parent := -1

	while idx < lines.size():
		var line := lines[idx].strip_edges()
		idx += 1

		if line == "" or line == "HIERARCHY":
			continue

		if line.begins_with("MOTION"):
			return idx - 1  # back up so MOTION line is re-read

		var tokens := line.replace("\t", " ").split(" ", false)
		if tokens.size() == 0:
			continue

		match tokens[0]:
			"ROOT", "JOINT":
				var joint := BVHJoint.new()
				joint.name = tokens[1] if tokens.size() > 1 else "Joint_%d" % _joints.size()
				joint.parent_index = current_parent
				var joint_idx := _joints.size()
				_joints.append(joint)
				# Current joint becomes the parent for children
				parent_stack.append(current_parent)
				current_parent = joint_idx

			"End":
				# "End Site"
				var joint := BVHJoint.new()
				joint.name = "EndSite_%d" % _joints.size()
				joint.parent_index = current_parent
				joint.is_end_site = true
				var joint_idx := _joints.size()
				_joints.append(joint)
				parent_stack.append(current_parent)
				current_parent = joint_idx

			"OFFSET":
				if _joints.size() > 0:
					var j := _joints[_joints.size() - 1]
					if tokens.size() >= 4:
						j.offset = Vector3(
							tokens[1].to_float(),
							tokens[2].to_float(),
							tokens[3].to_float())

			"CHANNELS":
				if _joints.size() > 0:
					var j := _joints[_joints.size() - 1]
					var n_channels := tokens[1].to_int() if tokens.size() > 1 else 0
					j.channel_start = _total_channels
					j.channels = PackedStringArray()
					for ci in range(2, mini(2 + n_channels, tokens.size())):
						j.channels.append(tokens[ci])
					_total_channels += n_channels

			"{":
				pass  # already handled by ROOT/JOINT/End

			"}":
				if parent_stack.size() > 0:
					current_parent = parent_stack.pop_back()

	return idx


# ===========================================================================
# Motion data parsing
# ===========================================================================

## Parse the MOTION section. Returns a dictionary with frame_count, frame_time, data.
func _parse_motion(lines: PackedStringArray, start: int) -> Variant:
	var idx := start
	var frame_count := 0
	var frame_time := 1.0 / 60.0
	var data: Array = []

	# Find MOTION header
	while idx < lines.size():
		var line := lines[idx].strip_edges()
		idx += 1
		if line.begins_with("MOTION"):
			break

	# Read frame count and frame time
	while idx < lines.size():
		var line := lines[idx].strip_edges()
		idx += 1

		if line.begins_with("Frames:"):
			var parts := line.split(":", false)
			if parts.size() >= 2:
				frame_count = parts[1].strip_edges().to_int()
		elif line.begins_with("Frame Time:"):
			var parts := line.split(":", false)
			if parts.size() >= 2:
				frame_time = parts[1].strip_edges().to_float()
			break

	if frame_count == 0:
		push_error("BVHImporter: No frames found in MOTION section.")
		return null

	# Read channel data rows
	for f in range(frame_count):
		if idx >= lines.size():
			break
		var line := lines[idx].strip_edges()
		idx += 1

		# Skip blank lines
		while line == "" and idx < lines.size():
			line = lines[idx].strip_edges()
			idx += 1

		var tokens := line.replace("\t", " ").split(" ", false)

		var row := PackedFloat64Array()
		row.resize(tokens.size())
		for i in range(tokens.size()):
			row[i] = tokens[i].to_float()
		data.append(row)

	return {
		"frame_count": data.size(),  # use actual parsed count
		"frame_time": frame_time,
		"data": data,
	}


# ===========================================================================
# Rotation construction
# ===========================================================================

## Build a quaternion rotation from BVH Euler channels in the given order.
func _build_rotation(joint: BVHJoint, row: PackedFloat64Array,
		ch_start: int, rot_order: Array[String]) -> Quaternion:
	var result := Quaternion.IDENTITY

	# BVH applies rotations in the order they appear in the CHANNELS line,
	# which is typically parent-to-child (extrinsic). We compose left-to-right.
	for ch_name in rot_order:
		# Find the channel index
		var ci := -1
		for i in range(joint.channels.size()):
			if joint.channels[i] == ch_name:
				ci = i
				break
		if ci < 0:
			continue

		var val_deg: float = row[ch_start + ci] if (ch_start + ci) < row.size() else 0.0
		var val_rad := deg_to_rad(val_deg)

		var axis_rot := Quaternion.IDENTITY
		match ch_name:
			"Xrotation":
				axis_rot = Quaternion(Vector3(1, 0, 0), val_rad)
			"Yrotation":
				axis_rot = Quaternion(Vector3(0, 1, 0), val_rad)
			"Zrotation":
				axis_rot = Quaternion(Vector3(0, 0, 1), val_rad)

		result = result * axis_rot

	return result.normalized()


# ===========================================================================
# Velocity computation
# ===========================================================================

## Compute bone and root velocities using central finite differences.
func _compute_velocities(db: MotionDatabase, frame_time: float) -> void:
	if db.frame_count < 2:
		return

	var inv_dt := 1.0 / (frame_time * 2.0)

	for f in range(db.frame_count):
		var prev := maxi(f - 1, 0)
		var next := mini(f + 1, db.frame_count - 1)
		var dt_scale := inv_dt
		if f == 0 or f == db.frame_count - 1:
			dt_scale = 1.0 / frame_time

		for bone in range(db.bone_count):
			var p0 := db.get_bone_position(prev, bone)
			var p1 := db.get_bone_position(next, bone)
			var vel := (p1 - p0) * dt_scale
			db.set_bone_velocity(f, bone, vel)

			# Angular velocity from quaternion difference
			var r0 := db.get_bone_rotation(prev, bone)
			var r1 := db.get_bone_rotation(next, bone)
			var dq := r1 * r0.inverse()
			if dq.w < 0.0:
				dq = Quaternion(-dq.x, -dq.y, -dq.z, -dq.w)
			var axis_angle := SpringUtils.quat_to_scaled_axis(dq)
			db.set_bone_angular_velocity(f, bone, axis_angle * dt_scale)

		# Root velocity
		var rp0 := db.get_root_position(prev)
		var rp1 := db.get_root_position(next)
		db.set_root_velocity(f, (rp1 - rp0) * dt_scale)


# ===========================================================================
# Utility
# ===========================================================================

## Find a bone index by checking name against a list of candidate names.
func _find_bone_by_name(bone_names: PackedStringArray, candidates: PackedStringArray) -> int:
	for i in range(bone_names.size()):
		var bn := bone_names[i].to_lower()
		for c in candidates:
			if bn == c.to_lower():
				return i
	# Partial match fallback
	for i in range(bone_names.size()):
		var bn := bone_names[i].to_lower()
		for c in candidates:
			if bn.contains(c.to_lower()) or c.to_lower().contains(bn):
				return i
	return -1
