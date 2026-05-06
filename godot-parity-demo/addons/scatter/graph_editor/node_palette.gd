## Popup menu listing all available ScatterNode types that can be added to the graph.
##
## Emits [signal node_type_selected] with the chosen type string and the
## position where the menu was opened (so the caller can place the new node).
@tool
class_name ScatterNodePalette
extends PopupMenu

signal node_type_selected(type_name: String, at_position: Vector2)

## Map from menu-item index to the node type identifier string.
var _type_map: Dictionary = {}

## Position where the popup was invoked (graph-local coordinates).
var _spawn_position: Vector2 = Vector2.ZERO


func _ready() -> void:
	_build_menu()
	id_pressed.connect(_on_id_pressed)


func _build_menu() -> void:
	clear()
	_type_map.clear()

	var types: Array[Dictionary] = [
		{"id": 0, "label": "Surface Sampler", "type": "SurfaceSampler"},
		{"id": 1, "label": "Slope Filter",    "type": "SlopeFilter"},
		{"id": 2, "label": "Random Transform", "type": "RandomTransform"},
		{"id": 3, "label": "Instance Placer",  "type": "InstancePlacer"},
	]

	for entry in types:
		add_item(entry["label"], entry["id"])
		_type_map[entry["id"]] = entry["type"]


## Open the palette at the given screen position, remembering the graph-local
## spawn point for the caller.
func open_at(screen_pos: Vector2, graph_pos: Vector2) -> void:
	_spawn_position = graph_pos
	position = Vector2i(screen_pos)
	popup()


func _on_id_pressed(id: int) -> void:
	if _type_map.has(id):
		node_type_selected.emit(_type_map[id], _spawn_position)
