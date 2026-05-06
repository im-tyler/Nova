class_name AudioGraphNode
extends Resource

## Base class for all audio graph nodes.

@export var node_name: String = "Node"
@export var graph_position: Vector2 = Vector2.ZERO
@export var input_count: int = 0
@export var output_count: int = 0


func _create_audio_effect() -> AudioEffect:
	## Override in subclasses to return the appropriate AudioEffect.
	return null


func get_node_title() -> String:
	return node_name


func get_node_color() -> Color:
	return Color(0.3, 0.3, 0.3)
