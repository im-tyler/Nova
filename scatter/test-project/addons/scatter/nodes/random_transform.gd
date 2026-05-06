## Applies random offset, rotation, and scale to each point's transform.
##
## All randomisation is seeded for deterministic results.
@tool
class_name RandomTransform
extends ScatterNode

## Maximum random offset per axis. Each axis is randomised in [-range, +range].
@export var offset_range: Vector3 = Vector3.ZERO

## Maximum random rotation per axis in degrees. Each axis in [-range, +range].
@export var rotation_range: Vector3 = Vector3(0.0, 360.0, 0.0)

## Uniform scale range. x = min scale, y = max scale.
@export var scale_range: Vector2 = Vector2(0.8, 1.2)

## Random seed.
@export var seed: int = 0


func _init() -> void:
	label = "Random Transform"


func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed

	for point in points:
		# Random offset
		var offset := Vector3(
			rng.randf_range(-offset_range.x, offset_range.x),
			rng.randf_range(-offset_range.y, offset_range.y),
			rng.randf_range(-offset_range.z, offset_range.z),
		)
		point.position += offset

		# Random rotation (applied as Euler angles)
		var rot := Vector3(
			deg_to_rad(rng.randf_range(-rotation_range.x, rotation_range.x)),
			deg_to_rad(rng.randf_range(-rotation_range.y, rotation_range.y)),
			deg_to_rad(rng.randf_range(-rotation_range.z, rotation_range.z)),
		)
		var basis := Basis.from_euler(rot)

		# Random uniform scale
		var s := rng.randf_range(scale_range.x, scale_range.y)

		# Compose transform: scale -> rotate -> translate
		var scaled_basis := basis.scaled(Vector3(s, s, s))
		point.transform = Transform3D(scaled_basis, point.position)

	return points
