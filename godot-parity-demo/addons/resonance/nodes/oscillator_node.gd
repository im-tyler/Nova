class_name OscillatorNode
extends AudioGraphNode

## Generates a tone using AudioStreamGenerator.
## This is a source node -- it does not produce an AudioEffect,
## but instead drives an AudioStreamPlayer with generated samples.

enum Waveform { SINE, SQUARE, SAW, TRIANGLE }

@export var frequency: float = 440.0
@export var waveform: Waveform = Waveform.SINE
@export var amplitude: float = 0.5


func _init() -> void:
	node_name = "Oscillator"
	input_count = 0
	output_count = 1


func get_node_title() -> String:
	return "Oscillator"


func get_node_color() -> Color:
	return Color(0.2, 0.6, 0.3)
