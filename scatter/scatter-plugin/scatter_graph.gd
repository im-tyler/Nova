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


## Serialize the graph (nodes + connections metadata) to a .tres resource file.
## Connection data is stored as metadata on the resource so it round-trips.
func save_to_file(path: String) -> Error:
	# Store node class names and positions for the editor to reconstruct.
	var node_data: Array[Dictionary] = []
	for node in nodes:
		if node == null:
			continue
		node_data.append({
			"type": node.get_class_name_custom(),
			"properties": node.serialize_properties(),
		})
	set_meta("scatter_node_data", node_data)
	return ResourceSaver.save(self, path)


## Load a graph from a .tres resource file.  Returns the loaded ScatterGraph
## or null on failure.
static func load_from_file(path: String) -> ScatterGraph:
	if not ResourceLoader.exists(path):
		push_warning("ScatterGraph: file not found '%s'" % path)
		return null
	var res := ResourceLoader.load(path)
	if res is ScatterGraph:
		return res as ScatterGraph
	push_warning("ScatterGraph: loaded resource is not a ScatterGraph.")
	return null
