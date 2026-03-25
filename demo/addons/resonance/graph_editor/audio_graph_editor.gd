@tool
extends VBoxContainer

## Visual audio graph editor built on GraphEdit.
## Provides a node palette, connection editing, and live preview.

const OscillatorNodeRes = preload("res://addons/resonance-plugin/nodes/oscillator_node.gd")
const GainNodeRes = preload("res://addons/resonance-plugin/nodes/gain_node.gd")
const FilterNodeRes = preload("res://addons/resonance-plugin/nodes/filter_node.gd")
const ReverbNodeRes = preload("res://addons/resonance-plugin/nodes/reverb_node.gd")
const OutputNodeRes = preload("res://addons/resonance-plugin/nodes/output_node.gd")

var graph_edit: GraphEdit = null
var audio_graph: AudioGraph = null
var _node_counter: int = 0
var _is_playing: bool = false

# Maps GraphEdit node name -> AudioGraph node index
var _visual_to_data: Dictionary = {}
# Maps AudioGraph node index -> GraphEdit node name
var _data_to_visual: Dictionary = {}


func _init() -> void:
	audio_graph = AudioGraph.new()


func _ready() -> void:
	set_process(false)
	_build_ui()


func _build_ui() -> void:
	# -- Toolbar --
	var toolbar := HBoxContainer.new()
	toolbar.name = "Toolbar"

	var add_label := Label.new()
	add_label.text = "Add: "
	toolbar.add_child(add_label)

	var node_types := ["Oscillator", "Gain", "Filter", "Reverb", "Output"]
	for type_name in node_types:
		var btn := Button.new()
		btn.text = type_name
		btn.pressed.connect(_on_add_node.bind(type_name))
		toolbar.add_child(btn)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	toolbar.add_child(spacer)

	var play_btn := Button.new()
	play_btn.text = "Play"
	play_btn.name = "PlayButton"
	play_btn.pressed.connect(_on_play_pressed)
	toolbar.add_child(play_btn)

	var stop_btn := Button.new()
	stop_btn.text = "Stop"
	stop_btn.name = "StopButton"
	stop_btn.pressed.connect(_on_stop_pressed)
	toolbar.add_child(stop_btn)

	var clear_btn := Button.new()
	clear_btn.text = "Clear"
	clear_btn.pressed.connect(_on_clear_pressed)
	toolbar.add_child(clear_btn)

	add_child(toolbar)

	# -- GraphEdit --
	graph_edit = GraphEdit.new()
	graph_edit.name = "GraphEdit"
	graph_edit.size_flags_vertical = Control.SIZE_EXPAND_FILL
	graph_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	graph_edit.connection_request.connect(_on_connection_request)
	graph_edit.disconnection_request.connect(_on_disconnection_request)
	graph_edit.delete_nodes_request.connect(_on_delete_nodes_request)
	add_child(graph_edit)

	# -- Status bar --
	var status := Label.new()
	status.name = "StatusLabel"
	status.text = "Resonance Audio Graph -- Add nodes and connect them."
	add_child(status)


func _on_add_node(type_name: String) -> void:
	var data_node: AudioGraphNode = null

	match type_name:
		"Oscillator":
			data_node = OscillatorNodeRes.new()
		"Gain":
			data_node = GainNodeRes.new()
		"Filter":
			data_node = FilterNodeRes.new()
		"Reverb":
			data_node = ReverbNodeRes.new()
		"Output":
			data_node = OutputNodeRes.new()

	if data_node == null:
		return

	var data_idx := audio_graph.add_node(data_node)
	var visual_node := _create_visual_node(data_node, data_idx)
	graph_edit.add_child(visual_node)

	_visual_to_data[visual_node.name] = data_idx
	_data_to_visual[data_idx] = visual_node.name

	_update_status("Added %s node." % type_name)


func _create_visual_node(data_node: AudioGraphNode, data_idx: int) -> GraphNode:
	var gn := GraphNode.new()
	_node_counter += 1
	gn.name = "node_%d" % _node_counter
	gn.title = data_node.get_node_title()
	gn.position_offset = Vector2(50 + data_idx * 200, 100)

	# Apply color via a stylebox
	var sb := StyleBoxFlat.new()
	var color := data_node.get_node_color()
	sb.bg_color = color
	sb.corner_radius_top_left = 4
	sb.corner_radius_top_right = 4
	sb.corner_radius_bottom_left = 4
	sb.corner_radius_bottom_right = 4
	gn.add_theme_stylebox_override("titlebar", sb)

	var selected_sb := sb.duplicate()
	selected_sb.border_color = Color(1, 1, 1, 0.8)
	selected_sb.border_width_top = 2
	selected_sb.border_width_bottom = 2
	selected_sb.border_width_left = 2
	selected_sb.border_width_right = 2
	gn.add_theme_stylebox_override("titlebar_selected", selected_sb)

	# Add parameter controls based on node type
	if data_node is OscillatorNode:
		_add_oscillator_controls(gn, data_node)
	elif data_node is GainNode:
		_add_gain_controls(gn, data_node)
	elif data_node is FilterNode:
		_add_filter_controls(gn, data_node)
	elif data_node is ReverbNode:
		_add_reverb_controls(gn, data_node)
	elif data_node is OutputNode:
		_add_output_controls(gn, data_node)

	# Set up slots after controls are added
	# We need at least one child (slot row) for each port
	_ensure_slot_count(gn, max(data_node.input_count, data_node.output_count))

	# Enable input/output slots on the first row
	for i in range(gn.get_child_count()):
		var has_input := i < data_node.input_count
		var has_output := i < data_node.output_count
		gn.set_slot(i,
			has_input, 0, Color(0.8, 0.8, 0.2),
			has_output, 0, Color(0.2, 0.8, 0.8))

	return gn


func _ensure_slot_count(gn: GraphNode, minimum: int) -> void:
	while gn.get_child_count() < minimum:
		var spacer := Control.new()
		spacer.custom_minimum_size = Vector2(0, 4)
		gn.add_child(spacer)


func _add_oscillator_controls(gn: GraphNode, osc: OscillatorNode) -> void:
	# Frequency slider
	var freq_hbox := HBoxContainer.new()
	var freq_label := Label.new()
	freq_label.text = "Freq:"
	freq_hbox.add_child(freq_label)
	var freq_slider := HSlider.new()
	freq_slider.min_value = 20.0
	freq_slider.max_value = 2000.0
	freq_slider.step = 1.0
	freq_slider.value = osc.frequency
	freq_slider.custom_minimum_size = Vector2(120, 0)
	freq_slider.value_changed.connect(func(val: float) -> void: osc.frequency = val)
	freq_hbox.add_child(freq_slider)
	var freq_value := Label.new()
	freq_value.text = str(int(osc.frequency))
	freq_slider.value_changed.connect(func(val: float) -> void: freq_value.text = str(int(val)))
	freq_hbox.add_child(freq_value)
	gn.add_child(freq_hbox)

	# Waveform selector
	var wave_hbox := HBoxContainer.new()
	var wave_label := Label.new()
	wave_label.text = "Wave:"
	wave_hbox.add_child(wave_label)
	var wave_option := OptionButton.new()
	wave_option.add_item("Sine", OscillatorNode.Waveform.SINE)
	wave_option.add_item("Square", OscillatorNode.Waveform.SQUARE)
	wave_option.add_item("Saw", OscillatorNode.Waveform.SAW)
	wave_option.add_item("Triangle", OscillatorNode.Waveform.TRIANGLE)
	wave_option.selected = osc.waveform
	wave_option.item_selected.connect(func(idx: int) -> void: osc.waveform = idx as OscillatorNode.Waveform)
	wave_hbox.add_child(wave_option)
	gn.add_child(wave_hbox)

	# Amplitude slider
	var amp_hbox := HBoxContainer.new()
	var amp_label := Label.new()
	amp_label.text = "Amp:"
	amp_hbox.add_child(amp_label)
	var amp_slider := HSlider.new()
	amp_slider.min_value = 0.0
	amp_slider.max_value = 1.0
	amp_slider.step = 0.01
	amp_slider.value = osc.amplitude
	amp_slider.custom_minimum_size = Vector2(120, 0)
	amp_slider.value_changed.connect(func(val: float) -> void: osc.amplitude = val)
	amp_hbox.add_child(amp_slider)
	gn.add_child(amp_hbox)


func _add_gain_controls(gn: GraphNode, gain: GainNode) -> void:
	var hbox := HBoxContainer.new()
	var lbl := Label.new()
	lbl.text = "Gain:"
	hbox.add_child(lbl)
	var slider := HSlider.new()
	slider.min_value = 0.0
	slider.max_value = 2.0
	slider.step = 0.01
	slider.value = gain.gain
	slider.custom_minimum_size = Vector2(120, 0)
	slider.value_changed.connect(func(val: float) -> void: gain.gain = val)
	hbox.add_child(slider)
	var val_label := Label.new()
	val_label.text = "%.2f" % gain.gain
	slider.value_changed.connect(func(val: float) -> void: val_label.text = "%.2f" % val)
	hbox.add_child(val_label)
	gn.add_child(hbox)


func _add_filter_controls(gn: GraphNode, filter: FilterNode) -> void:
	# Filter type
	var type_hbox := HBoxContainer.new()
	var type_label := Label.new()
	type_label.text = "Type:"
	type_hbox.add_child(type_label)
	var type_option := OptionButton.new()
	type_option.add_item("Lowpass", FilterNode.FilterType.LOWPASS)
	type_option.add_item("Highpass", FilterNode.FilterType.HIGHPASS)
	type_option.add_item("Bandpass", FilterNode.FilterType.BANDPASS)
	type_option.selected = filter.filter_type
	type_option.item_selected.connect(func(idx: int) -> void: filter.filter_type = idx as FilterNode.FilterType)
	type_hbox.add_child(type_option)
	gn.add_child(type_hbox)

	# Cutoff
	var cutoff_hbox := HBoxContainer.new()
	var cutoff_label := Label.new()
	cutoff_label.text = "Cutoff:"
	cutoff_hbox.add_child(cutoff_label)
	var cutoff_slider := HSlider.new()
	cutoff_slider.min_value = 20.0
	cutoff_slider.max_value = 20000.0
	cutoff_slider.step = 10.0
	cutoff_slider.value = filter.cutoff_frequency
	cutoff_slider.custom_minimum_size = Vector2(120, 0)
	cutoff_slider.value_changed.connect(func(val: float) -> void: filter.cutoff_frequency = val)
	cutoff_hbox.add_child(cutoff_slider)
	gn.add_child(cutoff_hbox)

	# Resonance
	var res_hbox := HBoxContainer.new()
	var res_label := Label.new()
	res_label.text = "Reso:"
	res_hbox.add_child(res_label)
	var res_slider := HSlider.new()
	res_slider.min_value = 0.1
	res_slider.max_value = 10.0
	res_slider.step = 0.1
	res_slider.value = filter.resonance
	res_slider.custom_minimum_size = Vector2(120, 0)
	res_slider.value_changed.connect(func(val: float) -> void: filter.resonance = val)
	res_hbox.add_child(res_slider)
	gn.add_child(res_hbox)


func _add_reverb_controls(gn: GraphNode, reverb: ReverbNode) -> void:
	var params := [
		["Room:", "room_size", 0.0, 1.0],
		["Damp:", "damping", 0.0, 1.0],
		["Wet:", "wet", 0.0, 1.0],
		["Dry:", "dry", 0.0, 1.0],
	]
	for p in params:
		var hbox := HBoxContainer.new()
		var lbl := Label.new()
		lbl.text = p[0]
		hbox.add_child(lbl)
		var slider := HSlider.new()
		slider.min_value = p[2]
		slider.max_value = p[3]
		slider.step = 0.01
		slider.value = reverb.get(p[1])
		slider.custom_minimum_size = Vector2(100, 0)
		var prop_name: String = p[1]
		slider.value_changed.connect(func(val: float) -> void: reverb.set(prop_name, val))
		hbox.add_child(slider)
		gn.add_child(hbox)


func _add_output_controls(gn: GraphNode, output: OutputNode) -> void:
	var hbox := HBoxContainer.new()
	var lbl := Label.new()
	lbl.text = "Bus:"
	hbox.add_child(lbl)
	var line_edit := LineEdit.new()
	line_edit.text = output.bus_name
	line_edit.custom_minimum_size = Vector2(100, 0)
	line_edit.text_changed.connect(func(val: String) -> void: output.bus_name = val)
	hbox.add_child(line_edit)
	gn.add_child(hbox)


func _on_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	graph_edit.connect_node(from_node, from_port, to_node, to_port)

	var from_idx: int = _visual_to_data.get(String(from_node), -1)
	var to_idx: int = _visual_to_data.get(String(to_node), -1)
	if from_idx >= 0 and to_idx >= 0:
		audio_graph.connect_nodes(from_idx, from_port, to_idx, to_port)
		_update_status("Connected %s -> %s" % [from_node, to_node])


func _on_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	graph_edit.disconnect_node(from_node, from_port, to_node, to_port)

	var from_idx: int = _visual_to_data.get(String(from_node), -1)
	var to_idx: int = _visual_to_data.get(String(to_node), -1)
	if from_idx >= 0 and to_idx >= 0:
		audio_graph.disconnect_nodes(from_idx, from_port, to_idx, to_port)
		_update_status("Disconnected %s -> %s" % [from_node, to_node])


func _on_delete_nodes_request(nodes_to_delete: Array[StringName]) -> void:
	# Collect indices to remove (in reverse to preserve ordering)
	var indices_to_remove: Array[int] = []
	for node_name in nodes_to_delete:
		var idx: int = _visual_to_data.get(String(node_name), -1)
		if idx >= 0:
			indices_to_remove.append(idx)

	indices_to_remove.sort()
	indices_to_remove.reverse()

	for idx in indices_to_remove:
		audio_graph.remove_node(idx)

	# Remove visual nodes
	for node_name in nodes_to_delete:
		var visual: Node = graph_edit.get_node(NodePath(String(node_name)))
		if visual:
			graph_edit.remove_child(visual)
			visual.queue_free()

	# Rebuild mappings
	_rebuild_mappings()
	_update_status("Deleted %d node(s)." % nodes_to_delete.size())


func _rebuild_mappings() -> void:
	_visual_to_data.clear()
	_data_to_visual.clear()
	var idx := 0
	for child in graph_edit.get_children():
		if child is GraphNode:
			_visual_to_data[child.name] = idx
			_data_to_visual[idx] = child.name
			idx += 1


func _process(_delta: float) -> void:
	if _is_playing and audio_graph:
		audio_graph.pump_audio()


func _on_play_pressed() -> void:
	if _is_playing:
		audio_graph.stop()

	var tree := get_tree()
	if tree == null:
		_update_status("Cannot play: no SceneTree available.")
		return

	audio_graph.execute(tree)
	_is_playing = true
	set_process(true)
	_update_status("Playing audio graph...")


func _on_stop_pressed() -> void:
	audio_graph.stop()
	_is_playing = false
	set_process(false)
	_update_status("Stopped.")


func _on_clear_pressed() -> void:
	_on_stop_pressed()

	# Remove all visual nodes
	var to_remove: Array[Node] = []
	for child in graph_edit.get_children():
		if child is GraphNode:
			to_remove.append(child)
	for node in to_remove:
		graph_edit.remove_child(node)
		node.queue_free()

	graph_edit.clear_connections()
	audio_graph.nodes.clear()
	audio_graph.connections.clear()
	_visual_to_data.clear()
	_data_to_visual.clear()
	_node_counter = 0
	_update_status("Graph cleared.")


func _update_status(msg: String) -> void:
	var status: Label = get_node_or_null("StatusLabel")
	if status:
		status.text = msg
