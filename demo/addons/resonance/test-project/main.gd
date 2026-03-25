extends Control

## Standalone test scene for the Resonance audio graph.
## Run this scene to test the audio graph without the editor plugin dock.

const AudioGraphEditorScript = preload("res://addons/resonance-plugin/graph_editor/audio_graph_editor.gd")

var graph_editor: Control = null


func _ready() -> void:
	graph_editor = AudioGraphEditorScript.new()
	graph_editor.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(graph_editor)

	# Add a default set of nodes for quick testing
	_setup_default_graph()


func _setup_default_graph() -> void:
	# Wait one frame so the editor is fully initialized
	await get_tree().process_frame

	# The user can add nodes interactively via the toolbar buttons.
	# This just sets up the scene; no auto-wiring so users see how it works.
	pass
