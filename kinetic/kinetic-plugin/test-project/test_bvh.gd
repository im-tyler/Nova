## BVH import test script.
## Loads a real BVH file, parses it, builds a MotionDatabase, and prints diagnostics.
## Attach to a Node3D in a scene, or run the scene with: godot --path . 2>&1
extends Node3D


func _ready() -> void:
	# Path to the BVH test file.
	var bvh_path := OS.get_environment("BVH_PATH")
	var candidates: PackedStringArray = PackedStringArray()
	if bvh_path != "":
		candidates.append(bvh_path)
	candidates.append("/Users/tyler/Documents/animation/test-data/walk.bvh")
	var project_dir := ProjectSettings.globalize_path("res://")
	candidates.append(project_dir.path_join("../../test-data/walk.bvh"))

	print("=== Kinetic BVH Import Test ===")

	var file: FileAccess = null
	for path in candidates:
		file = FileAccess.open(path, FileAccess.READ)
		if file:
			bvh_path = path
			break

	if not file:
		push_error("Could not open BVH file. Tried: %s" % str(candidates))
		print("FAIL: Could not open BVH file.")
		get_tree().quit(1)
		return

	print("BVH path: %s" % bvh_path)

	var text := file.get_as_text()
	file.close()
	print("File loaded: %d bytes, %d lines" % [text.length(), text.split("\n").size()])

	# Create importer and parse
	var importer := BVHImporter.new()
	print("")
	print("--- Parsing BVH ---")
	var db := importer.import_text(text, false, true)

	if db == null:
		print("FAIL: BVH import returned null.")
		get_tree().quit(1)
		return

	print("Frames:       %d" % db.frame_count)
	print("Bones:        %d" % db.bone_count)
	print("Bone names:   %s" % str(db.bone_names))
	print("Left foot:    %d (%s)" % [db.left_foot_bone,
		db.bone_names[db.left_foot_bone] if db.left_foot_bone >= 0 else "NOT FOUND"])
	print("Right foot:   %d (%s)" % [db.right_foot_bone,
		db.bone_names[db.right_foot_bone] if db.right_foot_bone >= 0 else "NOT FOUND"])
	print("Hip bone:     %d (%s)" % [db.hip_bone,
		db.bone_names[db.hip_bone] if db.hip_bone >= 0 else "NOT FOUND"])

	# Spot-check root trajectory
	print("")
	print("--- Root Trajectory Sample ---")
	for f in [0, db.frame_count / 4, db.frame_count / 2, db.frame_count - 1]:
		var pos := db.get_root_position(f)
		var dir := db.get_root_direction(f)
		var vel := db.get_root_velocity(f)
		print("  Frame %3d: pos=%s dir=%s vel=%s" % [f, str(pos), str(dir), str(vel)])

	# Spot-check bone transforms at frame 0
	print("")
	print("--- Bone Transforms (Frame 0) ---")
	for bone in range(mini(db.bone_count, 8)):
		var pos := db.get_bone_position(0, bone)
		var rot := db.get_bone_rotation(0, bone)
		print("  Bone %2d (%s): pos=%s rot=%s" % [
			bone, db.bone_names[bone], str(pos), str(rot)])

	# Build features
	print("")
	print("--- Building Features ---")
	db.build_features()

	var feature_count := 0
	if MotionDatabase.FEATURE_DIM > 0:
		feature_count = db.features.size() / MotionDatabase.FEATURE_DIM
	print("Feature vectors: %d" % feature_count)
	print("Feature dim:     %d" % MotionDatabase.FEATURE_DIM)
	print("Total floats:    %d" % db.features.size())

	# Print a sample feature vector
	if feature_count > 0:
		print("")
		print("--- Sample Feature Vector (Frame 0) ---")
		var fv := PackedFloat32Array()
		fv.resize(MotionDatabase.FEATURE_DIM)
		for d in range(MotionDatabase.FEATURE_DIM):
			fv[d] = db.features[d]
		print("  %s" % str(fv))

	# Test search
	if feature_count > 0:
		print("")
		print("--- Search Test ---")
		var query := PackedFloat32Array()
		query.resize(MotionDatabase.FEATURE_DIM)
		for d in range(MotionDatabase.FEATURE_DIM):
			query[d] = db.features[d]
		var match_frame := db.search(query, -1)
		print("  Query from frame 0 -> best match: frame %d" % match_frame)
		if match_frame >= 0:
			print("  PASS: Search returned a valid frame.")
		else:
			print("  WARN: Search returned -1 (no match found).")

	print("")
	print("=== BVH Import Test Complete ===")
	get_tree().quit(0)
