class_name ReverbNode
extends AudioGraphNode

## Wraps AudioEffectReverb.

@export_range(0.0, 1.0, 0.01) var room_size: float = 0.8
@export_range(0.0, 1.0, 0.01) var damping: float = 0.5
@export_range(0.0, 1.0, 0.01) var wet: float = 0.3
@export_range(0.0, 1.0, 0.01) var dry: float = 0.7


func _init() -> void:
	node_name = "Reverb"
	input_count = 1
	output_count = 1


func _create_audio_effect() -> AudioEffect:
	var effect := AudioEffectReverb.new()
	effect.room_size = room_size
	effect.damping = damping
	effect.wet = wet
	effect.dry = dry
	return effect


func get_node_title() -> String:
	return "Reverb"


func get_node_color() -> Color:
	return Color(0.5, 0.2, 0.6)
