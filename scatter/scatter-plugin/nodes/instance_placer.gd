## Takes a point set and creates a MultiMeshInstance3D with one instance per point.
##
## This is an output/terminal node. It still returns the point set unchanged so
## downstream inspection or chaining is possible, but its primary purpose is the
## side effect of building (or rebuilding) a MultiMeshInstance3D in the scene.
@tool
class_name InstancePlacer
extends ScatterNode

## The mesh to instance at every point.
@export var mesh: Mesh = null

## Optional material override applied to the MultiMeshInstance3D.
@export var material_override: Material = null

## The MultiMeshInstance3D that this node manages. Created on first execute if
## not already assigned.
var output_node: MultiMeshInstance3D = null

## Parent node under which the MultiMeshInstance3D will be added.
## Must be set before execute if output_node is null.
var parent: Node3D = null


func _init() -> void:
	label = "Instance Placer"


func get_class_name_custom() -> String:
	return "InstancePlacer"


func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	if mesh == null:
		push_warning("InstancePlacer: no mesh assigned.")
		return points

	_ensure_output_node()

	if output_node == null:
		push_warning("InstancePlacer: no output node and no parent to create one under.")
		return points

	# Build the MultiMesh
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = points.size()

	for i in range(points.size()):
		mm.set_instance_transform(i, points[i].transform)

	output_node.multimesh = mm

	if material_override != null:
		output_node.material_override = material_override

	return points


# -- Helpers ----------------------------------------------------------------

func _ensure_output_node() -> void:
	if output_node != null:
		return

	if parent == null:
		return

	output_node = MultiMeshInstance3D.new()
	output_node.name = "ScatterOutput"
	parent.add_child(output_node)

	# Make visible in editor scene tree
	if Engine.is_editor_hint() and output_node.get_parent() != null:
		output_node.owner = output_node.get_parent().owner
