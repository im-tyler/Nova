class_name OutputNode
extends AudioGraphNode

## Final output node. Routes audio to a named AudioServer bus.

@export var bus_name: String = "Master"


func _init() -> void:
	node_name = "Output"
	input_count = 1
	output_count = 0


func get_node_title() -> String:
	return "Output [%s]" % bus_name


func get_node_color() -> Color:
	return Color(0.7, 0.2, 0.2)
