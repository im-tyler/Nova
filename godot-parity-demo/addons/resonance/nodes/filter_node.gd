class_name FilterNode
extends AudioGraphNode

## Wraps AudioEffectFilter for lowpass, highpass, and bandpass filtering.

enum FilterType { LOWPASS, HIGHPASS, BANDPASS }

@export var filter_type: FilterType = FilterType.LOWPASS
@export_range(20.0, 20000.0, 1.0) var cutoff_frequency: float = 1000.0
@export_range(0.1, 10.0, 0.1) var resonance: float = 0.5


func _init() -> void:
	node_name = "Filter"
	input_count = 1
	output_count = 1


func _create_audio_effect() -> AudioEffect:
	var effect: AudioEffectFilter
	match filter_type:
		FilterType.LOWPASS:
			effect = AudioEffectLowPassFilter.new()
		FilterType.HIGHPASS:
			effect = AudioEffectHighPassFilter.new()
		FilterType.BANDPASS:
			effect = AudioEffectBandPassFilter.new()

	effect.cutoff_hz = cutoff_frequency
	effect.resonance = resonance
	return effect


func get_node_title() -> String:
	var type_str := "LP"
	match filter_type:
		FilterType.HIGHPASS:
			type_str = "HP"
		FilterType.BANDPASS:
			type_str = "BP"
	return "Filter (%s)" % type_str


func get_node_color() -> Color:
	return Color(0.2, 0.4, 0.7)
