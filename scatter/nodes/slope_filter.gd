## Filters points by surface normal slope relative to world up.
##
## Removes any point whose surface angle (degrees from vertical) falls outside
## the [min_angle, max_angle] range.
## 0 degrees = flat ground (normal pointing straight up).
## 90 degrees = vertical wall.
@tool
class_name SlopeFilter
extends ScatterNode

## Minimum slope angle in degrees (inclusive).
@export_range(0.0, 90.0, 0.1) var min_angle: float = 0.0

## Maximum slope angle in degrees (inclusive).
@export_range(0.0, 90.0, 0.1) var max_angle: float = 45.0


func _init() -> void:
	label = "Slope Filter"


func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	var result: Array[ScatterPoint] = []

	var min_rad := deg_to_rad(min_angle)
	var max_rad := deg_to_rad(max_angle)

	for point in points:
		var angle := point.normal.angle_to(Vector3.UP)
		if angle >= min_rad and angle <= max_rad:
			result.append(point)

	return result
