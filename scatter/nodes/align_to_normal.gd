## Rotates each point's transform to align its Y axis with the surface normal.
##
## A blend_factor of 0 leaves the transform unchanged. A blend_factor of 1
## fully aligns the local Y axis to the stored surface normal. Values in
## between produce a smooth blend using spherical linear interpolation.
@tool
class_name AlignToNormal
extends ScatterNode

## How strongly to align. 0 = no change, 1 = fully aligned to surface normal.
@export_range(0.0, 1.0, 0.01) var blend_factor: float = 1.0


func _init() -> void:
	label = "Align to Normal"


func get_class_name_custom() -> String:
	return "AlignToNormal"


func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	if blend_factor <= 0.0:
		return points

	for point in points:
		var current_basis := point.transform.basis
		var target_basis := _basis_aligned_to_normal(point.normal, current_basis)

		if blend_factor >= 1.0:
			point.transform.basis = target_basis
		else:
			# Spherical interpolation between current and target orientation.
			var current_quat := current_basis.get_rotation_quaternion()
			var target_quat := target_basis.get_rotation_quaternion()
			var blended_quat := current_quat.slerp(target_quat, blend_factor)
			var scale := current_basis.get_scale()
			point.transform.basis = Basis(blended_quat).scaled(scale)

	return points


## Build a Basis whose Y axis points along the given normal, preserving the
## existing basis scale. Forward (Z) direction is derived from the current
## basis to keep rotation around the normal axis stable.
static func _basis_aligned_to_normal(normal: Vector3, current_basis: Basis) -> Basis:
	var up := normal.normalized()
	if up.is_zero_approx():
		return current_basis

	# Derive a forward vector that is perpendicular to the new up.
	var forward := current_basis.z.normalized()
	# If forward is nearly parallel to up, pick an alternative.
	if absf(forward.dot(up)) > 0.999:
		forward = Vector3.FORWARD if absf(up.dot(Vector3.FORWARD)) < 0.999 else Vector3.RIGHT

	var right := up.cross(forward).normalized()
	forward = right.cross(up).normalized()

	var aligned := Basis(right, up, forward)
	# Preserve original scale.
	aligned = aligned.scaled(current_basis.get_scale())
	return aligned
