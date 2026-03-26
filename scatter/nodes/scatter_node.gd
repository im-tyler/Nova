## Base class for all nodes in a Scatter graph.
##
## Each node receives an array of ScatterPoint, processes it, and returns a
## (possibly different) array of ScatterPoint. Subclasses override [method execute].
@tool
class_name ScatterNode
extends Resource

## Human-readable label shown in the graph editor.
@export var label: String = "ScatterNode"

## Whether this node is enabled. Disabled nodes pass points through unchanged.
@export var enabled: bool = true


## Process the incoming point set and return the result.
## Override this in every concrete subclass.
func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	return points


## Return a custom class name string for serialization.
## Subclasses should override this.
func get_class_name_custom() -> String:
	return "ScatterNode"


## Serialize exported properties to a dictionary for save/load.
func serialize_properties() -> Dictionary:
	var data := {}
	for prop in get_property_list():
		if prop["usage"] & PROPERTY_USAGE_STORAGE and prop["usage"] & PROPERTY_USAGE_EDITOR:
			data[prop["name"]] = get(prop["name"])
	return data


## Restore exported properties from a serialized dictionary.
func deserialize_properties(data: Dictionary) -> void:
	for key in data.keys():
		if key in self:
			set(key, data[key])


## Utility: create a typed empty array ready to hold ScatterPoint values.
static func empty_point_array() -> Array[ScatterPoint]:
	var arr: Array[ScatterPoint] = []
	return arr
