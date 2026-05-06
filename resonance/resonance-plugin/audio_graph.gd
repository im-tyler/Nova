class_name AudioGraph
extends Resource

## Stores the audio graph topology and can execute it against AudioServer.

@export var nodes: Array[AudioGraphNode] = []
@export var connections: Array[Dictionary] = []
# Each connection: { "from_node": int, "from_port": int, "to_node": int, "to_port": int }

# Runtime state
var _bus_map: Dictionary = {}  # node index -> bus index
var _players: Array[AudioStreamPlayer] = []
var _player_osc_map: Dictionary = {}  # player index -> oscillator node index
var _active: bool = false


func add_node(node: AudioGraphNode) -> int:
	nodes.append(node)
	return nodes.size() - 1


func remove_node(index: int) -> void:
	# Remove connections referencing this node
	var new_connections: Array[Dictionary] = []
	for conn in connections:
		if conn["from_node"] != index and conn["to_node"] != index:
			var c := conn.duplicate()
			if c["from_node"] > index:
				c["from_node"] -= 1
			if c["to_node"] > index:
				c["to_node"] -= 1
			new_connections.append(c)
	connections = new_connections
	nodes.remove_at(index)


func connect_nodes(from_node: int, from_port: int, to_node: int, to_port: int) -> void:
	var conn := {
		"from_node": from_node,
		"from_port": from_port,
		"to_node": to_node,
		"to_port": to_port,
	}
	if conn not in connections:
		connections.append(conn)


func disconnect_nodes(from_node: int, from_port: int, to_node: int, to_port: int) -> void:
	var conn := {
		"from_node": from_node,
		"from_port": from_port,
		"to_node": to_node,
		"to_port": to_port,
	}
	var idx := connections.find(conn)
	if idx >= 0:
		connections.remove_at(idx)


func execute(scene_tree: SceneTree) -> void:
	## Build and activate the audio bus chain from the graph.
	stop()

	if nodes.is_empty():
		return

	# Find the output node
	var output_idx := -1
	for i in range(nodes.size()):
		if nodes[i] is OutputNode:
			output_idx = i
			break

	if output_idx < 0:
		push_warning("AudioGraph: No OutputNode found in graph.")
		return

	# Topological sort: walk backwards from output through connections
	var sorted_indices := _topological_sort(output_idx)
	if sorted_indices.is_empty():
		return

	# Create audio buses for effect nodes (non-source, non-output)
	# Strategy:
	#   - Source nodes (OscillatorNode) get an AudioStreamPlayer on a dedicated bus
	#   - Effect nodes (Gain, Filter, Reverb) add effects to the bus they receive input on
	#   - OutputNode routes to the Master bus
	#
	# Simplified model: single chain. Source -> effects -> output.
	# Each source gets its own bus with the chain of effects applied.

	var source_indices: Array[int] = []
	var effect_chain: Array[int] = []

	for idx in sorted_indices:
		if nodes[idx] is OscillatorNode:
			source_indices.append(idx)
		elif nodes[idx] is OutputNode:
			pass  # handled separately
		else:
			effect_chain.append(idx)

	# Determine effect order: effects should be in sorted order (source -> ... -> output)
	# effect_chain is already in topological order

	for src_idx in source_indices:
		var bus_name := "Resonance_%d" % src_idx
		var bus_idx := AudioServer.bus_count
		AudioServer.add_bus(bus_idx)
		AudioServer.set_bus_name(bus_idx, bus_name)

		# Find the chain of effects from this source to output
		var chain := _get_effect_chain(src_idx, output_idx)

		# Apply effects to the bus
		for effect_node_idx in chain:
			var effect := nodes[effect_node_idx]._create_audio_effect()
			if effect:
				var effect_idx := AudioServer.get_bus_effect_count(bus_idx)
				AudioServer.add_bus_effect(bus_idx, effect, effect_idx)

		# Set bus send to Master (OutputNode target)
		var output_bus := nodes[output_idx].bus_name if nodes[output_idx] is OutputNode else "Master"
		AudioServer.set_bus_send(bus_idx, output_bus)

		# Apply gain from any GainNode in the chain
		for effect_node_idx in chain:
			if nodes[effect_node_idx] is GainNode:
				var gain_db: float = linear_to_db(nodes[effect_node_idx].gain)
				AudioServer.set_bus_volume_db(bus_idx, gain_db)
				break  # apply first gain node to bus volume

		_bus_map[src_idx] = bus_idx

		# Create AudioStreamPlayer for the oscillator
		var osc_node: OscillatorNode = nodes[src_idx] as OscillatorNode
		var player := AudioStreamPlayer.new()
		var generator := AudioStreamGenerator.new()
		generator.mix_rate = 44100.0
		generator.buffer_length = 0.1
		player.stream = generator
		player.bus = bus_name

		scene_tree.root.add_child(player)
		player.play()

		# Fill the generator buffer with the waveform
		var playback: AudioStreamGeneratorPlayback = player.get_stream_playback()
		_fill_oscillator_buffer(playback, osc_node, generator.mix_rate)

		var player_idx := _players.size()
		_player_osc_map[player_idx] = src_idx
		_players.append(player)

	_active = true


func stop() -> void:
	## Tear down the audio chain.
	for player in _players:
		if is_instance_valid(player):
			player.stop()
			player.get_parent().remove_child(player)
			player.queue_free()
	_players.clear()
	_player_osc_map.clear()

	# Remove created buses (iterate in reverse to avoid index shift)
	var indices_to_remove: Array[int] = []
	for key in _bus_map:
		indices_to_remove.append(_bus_map[key])
	indices_to_remove.sort()
	indices_to_remove.reverse()
	for bus_idx in indices_to_remove:
		if bus_idx > 0 and bus_idx < AudioServer.bus_count:
			AudioServer.remove_bus(bus_idx)
	_bus_map.clear()
	_active = false


func _topological_sort(output_idx: int) -> Array[int]:
	## Simple BFS backwards from output to find all reachable nodes in order.
	var visited: Dictionary = {}
	var order: Array[int] = []
	var queue: Array[int] = [output_idx]

	while not queue.is_empty():
		var current: int = queue.pop_front()
		if current in visited:
			continue
		visited[current] = true
		order.append(current)

		# Find all nodes connected TO this node
		for conn in connections:
			if conn["to_node"] == current and conn["from_node"] not in visited:
				queue.append(conn["from_node"])

	order.reverse()  # sources first, output last
	return order


func _get_effect_chain(source_idx: int, output_idx: int) -> Array[int]:
	## Walk from source to output, collecting intermediate effect nodes.
	var chain: Array[int] = []
	var current := source_idx
	var visited: Dictionary = {}

	while current != output_idx:
		visited[current] = true
		var found_next := false
		for conn in connections:
			if conn["from_node"] == current and conn["to_node"] not in visited:
				var next_idx: int = conn["to_node"]
				if next_idx != output_idx:
					chain.append(next_idx)
				current = next_idx
				found_next = true
				break
		if not found_next:
			break

	return chain


func pump_audio() -> void:
	## Call this every frame to keep oscillator buffers filled.
	if not _active:
		return
	for i in range(_players.size()):
		var player := _players[i]
		if not is_instance_valid(player) or not player.playing:
			continue
		var playback := player.get_stream_playback() as AudioStreamGeneratorPlayback
		if playback == null or playback.get_frames_available() <= 0:
			continue
		var osc_idx: int = _player_osc_map.get(i, -1)
		if osc_idx < 0 or osc_idx >= nodes.size():
			continue
		var osc := nodes[osc_idx] as OscillatorNode
		if osc == null:
			continue
		_fill_oscillator_buffer_continuous(playback, osc, 44100.0)


func _fill_oscillator_buffer_continuous(playback: AudioStreamGeneratorPlayback, osc: OscillatorNode, sample_rate: float) -> void:
	var frames := playback.get_frames_available()
	if frames <= 0:
		return

	var phase: float = osc.get_meta("_phase") if osc.has_meta("_phase") else 0.0
	var increment := osc.frequency / sample_rate
	var amplitude := osc.amplitude

	for i in range(frames):
		var sample := 0.0
		match osc.waveform:
			OscillatorNode.Waveform.SINE:
				sample = sin(phase * TAU) * amplitude
			OscillatorNode.Waveform.SQUARE:
				sample = (1.0 if fmod(phase, 1.0) < 0.5 else -1.0) * amplitude
			OscillatorNode.Waveform.SAW:
				sample = (2.0 * fmod(phase, 1.0) - 1.0) * amplitude
			OscillatorNode.Waveform.TRIANGLE:
				var t := fmod(phase, 1.0)
				sample = (4.0 * abs(t - 0.5) - 1.0) * amplitude

		playback.push_frame(Vector2(sample, sample))
		phase += increment
		if phase > 1.0:
			phase -= 1.0

	osc.set_meta("_phase", phase)


func _fill_oscillator_buffer(playback: AudioStreamGeneratorPlayback, osc: OscillatorNode, sample_rate: float) -> void:
	## Fill the generator playback buffer with one pass of audio data.
	var frames_available := playback.get_frames_available()
	var phase := 0.0
	var increment := osc.frequency / sample_rate
	var amplitude := osc.amplitude

	for i in range(frames_available):
		var sample := 0.0
		match osc.waveform:
			OscillatorNode.Waveform.SINE:
				sample = sin(phase * TAU) * amplitude
			OscillatorNode.Waveform.SQUARE:
				sample = (1.0 if fmod(phase, 1.0) < 0.5 else -1.0) * amplitude
			OscillatorNode.Waveform.SAW:
				sample = (2.0 * fmod(phase, 1.0) - 1.0) * amplitude
			OscillatorNode.Waveform.TRIANGLE:
				var t := fmod(phase, 1.0)
				sample = (4.0 * abs(t - 0.5) - 1.0) * amplitude

		playback.push_frame(Vector2(sample, sample))
		phase += increment

	# Store phase for continuous generation
	osc.set_meta("_phase", phase)
