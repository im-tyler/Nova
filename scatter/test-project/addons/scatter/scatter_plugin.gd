## Scatter EditorPlugin -- provides a visual graph editor for building and
## executing ScatterNode pipelines inside the Godot editor.
##
## Adds a bottom panel containing a ScatterGraphEditor (GraphEdit-based) with
## a toolbar for adding nodes, executing the graph, and clearing it.
@tool
extends EditorPlugin

const DOCK_TITLE := "Scatter"

var _main_panel: VBoxContainer = null
var _graph_editor: ScatterGraphEditor = null
var _info_label: Label = null
var _add_menu: MenuButton = null


func _enter_tree() -> void:
	_main_panel = _build_main_panel()
	add_control_to_bottom_panel(_main_panel, DOCK_TITLE)


func _exit_tree() -> void:
	if _main_panel:
		remove_control_from_bottom_panel(_main_panel)
		_main_panel.queue_free()
		_main_panel = null


## ---- UI Construction -------------------------------------------------------

func _build_main_panel() -> VBoxContainer:
	var root := VBoxContainer.new()
	root.name = DOCK_TITLE
	root.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	root.custom_minimum_size = Vector2(0, 350)

	# -- Toolbar --
	var toolbar := HBoxContainer.new()
	toolbar.name = "Toolbar"

	# Title
	var title_label := Label.new()
	title_label.text = "Scatter PCG"
	title_label.add_theme_font_size_override("font_size", 16)
	toolbar.add_child(title_label)

	# Separator
	toolbar.add_child(VSeparator.new())

	# Add Node dropdown
	_add_menu = MenuButton.new()
	_add_menu.text = "Add Node"
	_add_menu.flat = false
	var popup := _add_menu.get_popup()
	popup.add_item("Surface Sampler", 0)
	popup.add_item("Slope Filter", 1)
	popup.add_item("Random Transform", 2)
	popup.add_item("Instance Placer", 3)
	popup.id_pressed.connect(_on_add_menu_id_pressed)
	toolbar.add_child(_add_menu)

	# Separator
	toolbar.add_child(VSeparator.new())

	# Execute button
	var execute_btn := Button.new()
	execute_btn.text = "Execute"
	execute_btn.pressed.connect(_on_execute_pressed)
	toolbar.add_child(execute_btn)

	# Clear button
	var clear_btn := Button.new()
	clear_btn.text = "Clear"
	clear_btn.pressed.connect(_on_clear_pressed)
	toolbar.add_child(clear_btn)

	# Spacer
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	toolbar.add_child(spacer)

	# Info label (right side of toolbar)
	_info_label = Label.new()
	_info_label.text = "Ready"
	_info_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_info_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	toolbar.add_child(_info_label)

	root.add_child(toolbar)

	# -- Separator between toolbar and graph --
	root.add_child(HSeparator.new())

	# -- Graph Editor --
	_graph_editor = ScatterGraphEditor.new()
	_graph_editor.name = "ScatterGraphEditor"
	_graph_editor.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_graph_editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_graph_editor.execution_finished.connect(_on_execution_finished)
	root.add_child(_graph_editor)

	return root


## ---- Toolbar actions -------------------------------------------------------

## Maps menu IDs to node type strings and a stagger offset so nodes added
## from the toolbar don't pile up at origin.
var _toolbar_add_offset: float = 0.0

func _on_add_menu_id_pressed(id: int) -> void:
	var type_name: String
	match id:
		0: type_name = "SurfaceSampler"
		1: type_name = "SlopeFilter"
		2: type_name = "RandomTransform"
		3: type_name = "InstancePlacer"
		_: return

	var node: ScatterNode = _create_node_by_type(type_name)
	if node == null:
		return

	# Place at a staggered position so sequential adds don't overlap.
	var pos := Vector2(40 + _toolbar_add_offset, 40 + _toolbar_add_offset)
	_toolbar_add_offset += 30.0
	if _toolbar_add_offset > 300.0:
		_toolbar_add_offset = 0.0

	_graph_editor.add_scatter_node(node, pos)
	_info_label.text = "Added: %s" % node.label


func _on_execute_pressed() -> void:
	if _graph_editor == null:
		return

	if _graph_editor._node_map.is_empty():
		_info_label.text = "Graph is empty -- add nodes first."
		return

	_graph_editor.execute_graph()


func _on_clear_pressed() -> void:
	if _graph_editor == null:
		return
	_graph_editor.clear_graph()
	_toolbar_add_offset = 0.0
	_info_label.text = "Graph cleared."


func _on_execution_finished(point_count: int, elapsed_ms: float) -> void:
	_info_label.text = "Generated %d points in %.2f ms" % [point_count, elapsed_ms]


## ---- Helpers ---------------------------------------------------------------

func _create_node_by_type(type_name: String) -> ScatterNode:
	match type_name:
		"SurfaceSampler":
			return SurfaceSampler.new()
		"SlopeFilter":
			return SlopeFilter.new()
		"RandomTransform":
			return RandomTransform.new()
		"InstancePlacer":
			return InstancePlacer.new()
	return null
