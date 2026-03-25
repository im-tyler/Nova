## A single point in a scatter point set.
##
## Carries position, surface normal, and a full transform that accumulates
## modifications as the point flows through the graph.
class_name ScatterPoint
extends RefCounted

var position: Vector3 = Vector3.ZERO
var normal: Vector3 = Vector3.UP
var transform: Transform3D = Transform3D.IDENTITY


func _init(
	p_position: Vector3 = Vector3.ZERO,
	p_normal: Vector3 = Vector3.UP
) -> void:
	position = p_position
	normal = p_normal
	transform = Transform3D(Basis(), p_position)


## Returns a deep copy of this point.
func duplicate_point() -> ScatterPoint:
	var copy := ScatterPoint.new(position, normal)
	copy.transform = transform
	return copy
