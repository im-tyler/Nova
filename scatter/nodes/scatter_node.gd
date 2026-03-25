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


## Utility: create a typed empty array ready to hold ScatterPoint values.
static func empty_point_array() -> Array[ScatterPoint]:
	var arr: Array[ScatterPoint] = []
	return arr
