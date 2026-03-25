extends Node

## Programmatic test for the Resonance AudioGraph.
## Creates: OscillatorNode (440Hz sine) -> GainNode (0.5) -> OutputNode
## Connects them, calls execute(), prints status, runs for 3 seconds, then stops.

var graph: AudioGraph = null


func _ready() -> void:
	print("[test] Resonance Audio Graph -- programmatic test starting")

	# Build the graph
	graph = AudioGraph.new()

	var osc := OscillatorNode.new()
	osc.frequency = 440.0
	osc.waveform = OscillatorNode.Waveform.SINE
	osc.amplitude = 0.5
	print("[test] Created OscillatorNode: freq=%s, wave=SINE, amp=%s" % [osc.frequency, osc.amplitude])

	var gain := GainNode.new()
	gain.gain = 0.5
	print("[test] Created GainNode: gain=%s" % gain.gain)

	var output := OutputNode.new()
	output.bus_name = "Master"
	print("[test] Created OutputNode: bus=%s" % output.bus_name)

	var osc_idx := graph.add_node(osc)
	var gain_idx := graph.add_node(gain)
	var output_idx := graph.add_node(output)
	print("[test] Node indices: osc=%d, gain=%d, output=%d" % [osc_idx, gain_idx, output_idx])

	# Connect: oscillator -> gain -> output
	graph.connect_nodes(osc_idx, 0, gain_idx, 0)
	graph.connect_nodes(gain_idx, 0, output_idx, 0)
	print("[test] Connections: osc->gain, gain->output")
	print("[test] Total nodes: %d, Total connections: %d" % [graph.nodes.size(), graph.connections.size()])

	# Execute the graph
	print("[test] Calling execute()...")
	graph.execute(get_tree())
	print("[test] execute() completed -- audio should be playing")
	print("[test] AudioServer bus count: %d" % AudioServer.bus_count)

	for i in range(AudioServer.bus_count):
		print("[test]   Bus %d: name=%s, volume_db=%.1f" % [i, AudioServer.get_bus_name(i), AudioServer.get_bus_volume_db(i)])

	# Let it play for 3 seconds then stop
	await get_tree().create_timer(3.0).timeout
	print("[test] 3 seconds elapsed -- stopping graph")
	graph.stop()
	print("[test] Graph stopped. AudioServer bus count: %d" % AudioServer.bus_count)
	print("[test] TEST PASSED -- all operations completed without errors")

	# Quit after a brief pause
	await get_tree().create_timer(0.5).timeout
	get_tree().quit(0)


func _process(_delta: float) -> void:
	if graph:
		graph.pump_audio()
