## Kinetic editor plugin — adds the Motion Matching dock to the Godot editor.
##
## Provides a UI panel for importing BVH files, inspecting MotionDatabase resources,
## and configuring motion matching parameters.
@tool
class_name KineticPlugin
extends EditorPlugin

const DOCK_TITLE := "Kinetic"

var _dock: Control


func _enter_tree() -> void:
	_dock = _build_dock()
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, _dock)
	print("[Kinetic] Motion Matching plugin loaded.")


func _exit_tree() -> void:
	if _dock:
		remove_control_from_docks(_dock)
		_dock.queue_free()
		_dock = null
	print("[Kinetic] Motion Matching plugin unloaded.")


# ===========================================================================
# Dock UI
# ===========================================================================

func _build_dock() -> Control:
	var panel := VBoxContainer.new()
	panel.name = DOCK_TITLE

	# --- Header ---
	var header := Label.new()
	header.text = "Kinetic — Motion Matching"
	header.add_theme_font_size_override("font_size", 16)
	panel.add_child(header)

	var sep := HSeparator.new()
	panel.add_child(sep)

	# --- Import section ---
	var import_label := Label.new()
	import_label.text = "Import BVH"
	panel.add_child(import_label)

	var import_row := HBoxContainer.new()
	panel.add_child(import_row)

	var path_edit := LineEdit.new()
	path_edit.name = "BVHPathEdit"
	path_edit.placeholder_text = "Path to .bvh file..."
	path_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	import_row.add_child(path_edit)

	var browse_btn := Button.new()
	browse_btn.text = "..."
	browse_btn.pressed.connect(_on_browse_pressed.bind(path_edit))
	import_row.add_child(browse_btn)

	var import_btn := Button.new()
	import_btn.text = "Import"
	import_btn.pressed.connect(_on_import_pressed.bind(path_edit))
	panel.add_child(import_btn)

	# --- Database info ---
	var sep2 := HSeparator.new()
	panel.add_child(sep2)

	var info_label := Label.new()
	info_label.name = "InfoLabel"
	info_label.text = "No database loaded."
	info_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	panel.add_child(info_label)

	# --- Build features button ---
	var build_btn := Button.new()
	build_btn.text = "Rebuild Features"
	build_btn.pressed.connect(_on_rebuild_features_pressed)
	panel.add_child(build_btn)

	return panel


# ===========================================================================
# Callbacks
# ===========================================================================

func _on_browse_pressed(path_edit: LineEdit) -> void:
	var dialog := EditorFileDialog.new()
	dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
	dialog.add_filter("*.bvh", "BVH Motion Capture Files")
	dialog.file_selected.connect(func(path: String):
		path_edit.text = path
		dialog.queue_free()
	)
	dialog.canceled.connect(func():
		dialog.queue_free()
	)
	EditorInterface.get_base_control().add_child(dialog)
	dialog.popup_centered(Vector2i(600, 400))


func _on_import_pressed(path_edit: LineEdit) -> void:
	var path := path_edit.text.strip_edges()
	if path == "":
		_set_info("No path specified.")
		return

	_set_info("Importing '%s'..." % path)

	var importer := BVHImporter.new()
	var db := importer.import_file(path, true, true)
	if not db:
		_set_info("Import failed. Check the output log for details.")
		return

	# Save as .tres next to the BVH file
	var save_path := path.get_basename() + ".tres"
	if path.begins_with("/") or path.find("://") >= 0:
		# Absolute path — save next to it if in project, else use res://
		if not path.begins_with("res://"):
			save_path = "res://imported_motion.tres"

	var err := ResourceSaver.save(db, save_path)
	if err != OK:
		_set_info("Import succeeded but failed to save resource (error %d)." % err)
		return

	_set_info("Imported: %d frames, %d bones, %d features.\nSaved to: %s" % [
		db.frame_count, db.bone_count, db.features.size() / MotionDatabase.FEATURE_DIM,
		save_path])

	EditorInterface.get_resource_filesystem().scan()


func _on_rebuild_features_pressed() -> void:
	var edited := EditorInterface.get_inspector().get_edited_object()
	if edited is MotionDatabase:
		var db: MotionDatabase = edited
		db.build_features()
		_set_info("Rebuilt features: %d frames x %d dims." % [
			db.frame_count, MotionDatabase.FEATURE_DIM])
	else:
		_set_info("Select a MotionDatabase resource in the inspector first.")


func _set_info(text: String) -> void:
	if not _dock:
		return
	var label := _dock.find_child("InfoLabel", true, false) as Label
	if label:
		label.text = text
