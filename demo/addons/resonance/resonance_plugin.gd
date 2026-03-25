@tool
extends EditorPlugin

const AudioGraphEditor = preload("res://addons/resonance-plugin/graph_editor/audio_graph_editor.gd")

var dock: Control = null


func _enter_tree() -> void:
	dock = AudioGraphEditor.new()
	dock.name = "AudioGraph"
	dock.custom_minimum_size = Vector2(400, 300)
	add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock)


func _exit_tree() -> void:
	if dock:
		remove_control_from_docks(dock)
		dock.queue_free()
		dock = null
