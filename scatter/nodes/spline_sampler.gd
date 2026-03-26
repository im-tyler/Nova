## Samples points along a Path3D spline at regular intervals.
##
## This is a generator node -- it ignores incoming points and produces a new
## set by distributing points along the spline curve, with optional random
## offset perpendicular to the path direction.
@tool
class_name SplineSampler
extends ScatterNode

## Number of points to distribute along the spline.
@export var point_count: int = 50

## Maximum random offset distance from the spline (perpendicular to path direction).
@export var offset_range: float = 0.0

## Random seed for the offset randomisation.
@export var seed: int = 0

## Path to the Path3D node whose curve will be sampled.
## Resolved at execute time relative to the scene tree root.
@export var path_node_path: NodePath = NodePath("")

## Cached reference -- set externally or resolved from path_node_path.
var path_node: Path3D = null


func _init() -> void:
	label = "Spline Sampler"


func get_class_name_custom() -> String:
	return "SplineSampler"


func execute(_points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	var result: Array[ScatterPoint] = []

	if path_node == null:
		push_warning("SplineSampler: no Path3D assigned.")
		return result

	var curve := path_node.curve
	if curve == null or curve.point_count < 2:
		push_warning("SplineSampler: curve is null or has fewer than 2 points.")
		return result

	var baked_length := curve.get_baked_length()
	if baked_length <= 0.0:
		push_warning("SplineSampler: curve has zero baked length.")
		return result

	var rng := RandomNumberGenerator.new()
	rng.seed = seed

	for i in range(point_count):
		# Distribute evenly along the spline length.
		var t: float
		if point_count <= 1:
			t = 0.0
		else:
			t = float(i) / float(point_count - 1)

		var offset_along := t * baked_length
		var pos := curve.sample_baked(offset_along)
		var up := curve.sample_baked_up_vector(offset_along)

		# Compute tangent from a small delta along the curve.
		var delta := 0.01
		var pos_ahead := curve.sample_baked(minf(offset_along + delta, baked_length))
		var tangent := (pos_ahead - pos).normalized()
		if tangent.is_zero_approx():
			tangent = Vector3.FORWARD

		# Normal is the up vector; side is perpendicular to tangent and up.
		var normal := up.normalized()
		if normal.is_zero_approx():
			normal = Vector3.UP
		var side := tangent.cross(normal).normalized()
		if side.is_zero_approx():
			side = Vector3.RIGHT

		# Apply random offset perpendicular to the spline direction.
		if offset_range > 0.0:
			var rand_side := rng.randf_range(-offset_range, offset_range)
			var rand_up := rng.randf_range(-offset_range, offset_range)
			pos += side * rand_side + normal * rand_up

		# Transform the position into world space using the Path3D's transform.
		pos = path_node.global_transform * pos

		var point := ScatterPoint.new(pos, normal)
		result.append(point)

	return result
