## Visual graph editor for the Scatter PCG pipeline.
##
## Extends GraphEdit to display ScatterNode resources as draggable GraphNode
## elements.  Users can add nodes via right-click context menu, connect ports to
## define the execution chain, and press Execute to run the graph.
##
## Point counts are shown on each connection after execution.
@tool
class_name ScatterGraphEditor
extends GraphEdit

## Emitted after a successful execution so external UI can update.
signal execution_finished(point_count: int, elapsed_ms: float)

## Internal auto-incrementing ID for graph nodes.
var _next_node_id: int = 0

## Maps node_id -> ScatterGraphNodeUI.
var _node_map: Dictionary = {}

## Connections stored as an array of dictionaries:
##   { "from_id": int, "to_id": int }
## Rebuilt whenever the user makes or breaks a connection.
var _edge_list: Array[Dictionary] = []

## Point counts per edge after the last execution.
## Key = "from_id->to_id", Value = int.
var _edge_point_counts: Dictionary = {}

## The palette popup (right-click menu).
var _palette: ScatterNodePalette = null

## Labels overlaid on connections to show point counts.
var _connection_labels: Array[Label] = []


func _ready() -> void:
	# GraphEdit configuration
	right_disconnects = true
	minimap_enabled = false
	show_grid = true

	# Context menu
	_palette = ScatterNodePalette.new()
	add_child(_palette)
	_palette.node_type_selected.connect(_on_palette_node_selected)

	# Signals
	connection_request.connect(_on_connection_request)
	disconnection_request.connect(_on_disconnection_request)
	delete_nodes_request.connect(_on_delete_nodes_request)


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			# Open node palette at click position
			var screen_pos := get_screen_position() + mb.position
			var graph_pos := (get_scroll_offset() + mb.position) / zoom
			_palette.open_at(screen_pos, graph_pos)
			accept_event()


## ---- Public API -----------------------------------------------------------

## Add a ScatterNode to the visual graph at the given position.
## Returns the created ScatterGraphNodeUI.
func add_scatter_node(scatter_node: ScatterNode, at_position: Vector2 = Vector2.ZERO) -> ScatterGraphNodeUI:
	var ui := ScatterGraphNodeUI.new()
	var id := _next_node_id
	_next_node_id += 1

	ui.name = "scatter_node_%d" % id
	ui.setup(scatter_node, id)
	ui.position_offset = at_position

	add_child(ui)
	_node_map[id] = ui

	return ui


## Remove a scatter node by its ID.
func remove_scatter_node(id: int) -> void:
	if not _node_map.has(id):
		return

	var ui: ScatterGraphNodeUI = _node_map[id]

	# Remove any connections involving this node
	var node_name := ui.name
	var to_remove: Array[Dictionary] = []
	for conn in get_connection_list():
		if conn["from_node"] == StringName(node_name) or conn["to_node"] == StringName(node_name):
			to_remove.append(conn)
	for conn in to_remove:
		disconnect_node(conn["from_node"], conn["from_port"], conn["to_node"], conn["to_port"])

	_node_map.erase(id)
	ui.queue_free()
	_rebuild_edge_list()


## Clear the entire graph.
func clear_graph() -> void:
	clear_connections()
	for id in _node_map.keys():
		var ui: ScatterGraphNodeUI = _node_map[id]
		ui.queue_free()
	_node_map.clear()
	_edge_list.clear()
	_edge_point_counts.clear()
	_clear_connection_labels()
	_next_node_id = 0


## Execute the graph by topologically sorting connected nodes and running
## them in order.  Returns the final point array.
func execute_graph() -> Array[ScatterPoint]:
	_clear_connection_labels()
	_edge_point_counts.clear()

	# Build adjacency from current connections.
	_rebuild_edge_list()

	# Topological sort -- find source nodes (no incoming edges), then BFS.
	var in_degree: Dictionary = {}  # node_id -> int
	var adj: Dictionary = {}        # node_id -> Array[int]

	for id in _node_map.keys():
		in_degree[id] = 0
		adj[id] = []

	for edge in _edge_list:
		adj[edge["from_id"]].append(edge["to_id"])
		in_degree[edge["to_id"]] += 1

	var queue: Array[int] = []
	for id in _node_map.keys():
		if in_degree[id] == 0:
			queue.append(id)

	var sorted_ids: Array[int] = []
	while not queue.is_empty():
		var current: int = queue.pop_front()
		sorted_ids.append(current)
		for neighbor in adj[current]:
			in_degree[neighbor] -= 1
			if in_degree[neighbor] == 0:
				queue.append(neighbor)

	# Execute in topological order.  Each node receives points from the
	# single incoming edge (if any).  For nodes with no input edge the
	# incoming array is empty (generators produce their own points).
	var node_output: Dictionary = {}  # node_id -> Array[ScatterPoint]

	var start_usec := Time.get_ticks_usec()

	for id in sorted_ids:
		var ui: ScatterGraphNodeUI = _node_map[id]
		var incoming_points: Array[ScatterPoint] = []

		# Gather input from connected predecessors.
		for edge in _edge_list:
			if edge["to_id"] == id and node_output.has(edge["from_id"]):
				incoming_points = node_output[edge["from_id"]]
				break  # single-input model

		var result: Array[ScatterPoint] = ui.scatter_node.execute(incoming_points)
		node_output[id] = result

		# Record point counts on outgoing edges.
		for edge in _edge_list:
			if edge["from_id"] == id:
				var key := "%d->%d" % [edge["from_id"], edge["to_id"]]
				_edge_point_counts[key] = result.size()

	var elapsed_usec := Time.get_ticks_usec() - start_usec
	var elapsed_ms := elapsed_usec / 1000.0

	# Find the terminal output (last in sorted order, or the one with no
	# outgoing edges).
	var final_points: Array[ScatterPoint] = []
	for id in sorted_ids:
		if adj[id].is_empty() and node_output.has(id):
			final_points = node_output[id]

	_show_connection_labels()

	execution_finished.emit(final_points.size(), elapsed_ms)
	return final_points


## ---- Signal handlers ------------------------------------------------------

func _on_palette_node_selected(type_name: String, at_position: Vector2) -> void:
	var node := _create_scatter_node(type_name)
	if node:
		add_scatter_node(node, at_position)


func _on_connection_request(from_node: StringName, from_port: int,
		to_node: StringName, to_port: int) -> void:
	# Prevent connecting a node to itself.
	if from_node == to_node:
		return
	connect_node(from_node, from_port, to_node, to_port)
	_rebuild_edge_list()


func _on_disconnection_request(from_node: StringName, from_port: int,
		to_node: StringName, to_port: int) -> void:
	disconnect_node(from_node, from_port, to_node, to_port)
	_rebuild_edge_list()


func _on_delete_nodes_request(nodes: Array[StringName]) -> void:
	for node_name in nodes:
		var ui := get_node_or_null(NodePath(node_name))
		if ui is ScatterGraphNodeUI:
			remove_scatter_node((ui as ScatterGraphNodeUI).node_id)


## ---- Save / Load ----------------------------------------------------------

## Serialize the current graph state to a .tres file at the given path.
func save_graph_to_file(path: String) -> Error:
	var graph := ScatterGraph.new()

	# Build ordered list from topological sort or just ID order.
	var sorted_ids := _node_map.keys().duplicate()
	sorted_ids.sort()

	var id_to_index: Dictionary = {}
	for i in range(sorted_ids.size()):
		id_to_index[sorted_ids[i]] = i
		var ui: ScatterGraphNodeUI = _node_map[sorted_ids[i]]
		graph.nodes.append(ui.scatter_node)

	# Store positions and connections as metadata.
	var positions: Array[Dictionary] = []
	for id in sorted_ids:
		var ui: ScatterGraphNodeUI = _node_map[id]
		positions.append({
			"x": ui.position_offset.x,
			"y": ui.position_offset.y,
		})
	graph.set_meta("node_positions", positions)

	_rebuild_edge_list()
	var connections: Array[Dictionary] = []
	for edge in _edge_list:
		if id_to_index.has(edge["from_id"]) and id_to_index.has(edge["to_id"]):
			connections.append({
				"from": id_to_index[edge["from_id"]],
				"to": id_to_index[edge["to_id"]],
			})
	graph.set_meta("connections", connections)

	return ResourceSaver.save(graph, path)


## Load a graph from a .tres file and rebuild the visual editor.
func load_graph_from_file(path: String) -> Error:
	var graph := ScatterGraph.load_from_file(path)
	if graph == null:
		return ERR_FILE_CANT_READ

	clear_graph()

	# Reconstruct nodes.
	var positions: Array = []
	if graph.has_meta("node_positions"):
		positions = graph.get_meta("node_positions")

	var ui_nodes: Array[ScatterGraphNodeUI] = []
	for i in range(graph.nodes.size()):
		var scatter_node: ScatterNode = graph.nodes[i]
		if scatter_node == null:
			continue
		var pos := Vector2.ZERO
		if i < positions.size():
			pos = Vector2(positions[i]["x"], positions[i]["y"])
		var ui := add_scatter_node(scatter_node, pos)
		ui_nodes.append(ui)

	# Reconstruct connections.
	if graph.has_meta("connections"):
		var connections: Array = graph.get_meta("connections")
		for conn in connections:
			var from_idx: int = conn["from"]
			var to_idx: int = conn["to"]
			if from_idx < ui_nodes.size() and to_idx < ui_nodes.size():
				connect_node(
					ui_nodes[from_idx].name, 0,
					ui_nodes[to_idx].name, 0
				)
		_rebuild_edge_list()

	return OK


## ---- Internal helpers -----------------------------------------------------

func _create_scatter_node(type_name: String) -> ScatterNode:
	match type_name:
		"SurfaceSampler":
			return SurfaceSampler.new()
		"SplineSampler":
			return SplineSampler.new()
		"SlopeFilter":
			return SlopeFilter.new()
		"NoiseFilter":
			return NoiseFilter.new()
		"RandomTransform":
			return RandomTransform.new()
		"AlignToNormal":
			return AlignToNormal.new()
		"InstancePlacer":
			return InstancePlacer.new()
	push_warning("ScatterGraphEditor: unknown node type '%s'" % type_name)
	return null


## Rebuild _edge_list from the current GraphEdit connections.
func _rebuild_edge_list() -> void:
	_edge_list.clear()
	for conn in get_connection_list():
		var from_ui := get_node_or_null(NodePath(conn["from_node"]))
		var to_ui := get_node_or_null(NodePath(conn["to_node"]))
		if from_ui is ScatterGraphNodeUI and to_ui is ScatterGraphNodeUI:
			_edge_list.append({
				"from_id": (from_ui as ScatterGraphNodeUI).node_id,
				"to_id": (to_ui as ScatterGraphNodeUI).node_id,
			})


## Remove all connection count labels.
func _clear_connection_labels() -> void:
	for lbl in _connection_labels:
		if is_instance_valid(lbl):
			lbl.queue_free()
	_connection_labels.clear()


## Create overlay labels showing point counts on each connection.
func _show_connection_labels() -> void:
	_clear_connection_labels()

	for conn in get_connection_list():
		var from_ui := get_node_or_null(NodePath(conn["from_node"]))
		var to_ui := get_node_or_null(NodePath(conn["to_node"]))
		if not (from_ui is ScatterGraphNodeUI and to_ui is ScatterGraphNodeUI):
			continue

		var from_id: int = (from_ui as ScatterGraphNodeUI).node_id
		var to_id: int = (to_ui as ScatterGraphNodeUI).node_id
		var key := "%d->%d" % [from_id, to_id]

		if not _edge_point_counts.has(key):
			continue

		var count: int = _edge_point_counts[key]

		var lbl := Label.new()
		lbl.text = "%d pts" % count
		lbl.add_theme_font_size_override("font_size", 12)
		lbl.add_theme_color_override("font_color", Color(1.0, 1.0, 1.0))
		lbl.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
		lbl.add_theme_constant_override("shadow_offset_x", 1)
		lbl.add_theme_constant_override("shadow_offset_y", 1)

		# Position at midpoint between the two graph nodes.
		var from_center: Vector2 = (from_ui as Control).position_offset + (from_ui as Control).size * 0.5
		var to_center: Vector2 = (to_ui as Control).position_offset + (to_ui as Control).size * 0.5
		var mid := (from_center + to_center) * 0.5 * zoom - get_scroll_offset()

		lbl.position = mid
		lbl.z_index = 10
		add_child(lbl)
		_connection_labels.append(lbl)
