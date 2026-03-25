class_name GainNode
extends AudioGraphNode

## Volume control node. Wraps AudioEffectAmplify.

@export_range(0.0, 2.0, 0.01) var gain: float = 1.0


func _init() -> void:
	node_name = "Gain"
	input_count = 1
	output_count = 1


func _create_audio_effect() -> AudioEffect:
	var effect := AudioEffectAmplify.new()
	effect.volume_db = linear_to_db(gain)
	return effect


func get_node_title() -> String:
	return "Gain"


func get_node_color() -> Color:
	return Color(0.6, 0.6, 0.2)
