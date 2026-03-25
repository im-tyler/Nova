## A directed acyclic graph of ScatterNode resources.
##
## Stores an ordered list of nodes. Execution flows from index 0 (generator)
## through each successive node, piping the point set forward.
@tool
class_name ScatterGraph
extends Resource

## Ordered list of nodes in the graph. The first node should be a generator
## (e.g. SurfaceSampler), followed by filters/transforms, ending with an
## output node (e.g. InstancePlacer).
@export var nodes: Array[ScatterNode] = []


## Execute the full graph. Each node receives the output of the previous one.
## Returns the final point set (useful for inspection/debugging even though
## the output node typically produces side effects like creating a MultiMesh).
func execute() -> Array[ScatterPoint]:
	var points: Array[ScatterPoint] = []

	for node in nodes:
		if node == null:
			continue
		if not node.enabled:
			continue
		points = node.execute(points)

	return points


## Convenience: append a node to the end of the graph.
func add_node(node: ScatterNode) -> void:
	nodes.append(node)


## Remove a node by index.
func remove_node(index: int) -> void:
	if index >= 0 and index < nodes.size():
		nodes.remove_at(index)


## Clear all nodes from the graph.
func clear() -> void:
	nodes.clear()
