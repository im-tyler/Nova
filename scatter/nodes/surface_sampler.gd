## Samples random points on the surface of a MeshInstance3D.
##
## This is a generator node -- it ignores incoming points and produces a new
## set by randomly sampling triangles on the target mesh, weighted by area.
@tool
class_name SurfaceSampler
extends ScatterNode

## Number of points to scatter on the surface.
@export var point_count: int = 100

## Random seed. Deterministic output for the same seed.
@export var seed: int = 0

## Path to the MeshInstance3D whose surface will be sampled.
## Resolved at execute time relative to the scene tree root.
@export var mesh_path: NodePath = NodePath("")

## Cached reference -- set externally or resolved from mesh_path.
var mesh_instance: MeshInstance3D = null


func _init() -> void:
	label = "Surface Sampler"


func execute(_points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	var result: Array[ScatterPoint] = []

	var mesh_data := _get_mesh_arrays()
	if mesh_data.is_empty():
		push_warning("SurfaceSampler: no mesh data available.")
		return result

	var vertices: PackedVector3Array = mesh_data[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = mesh_data[Mesh.ARRAY_NORMAL] if mesh_data[Mesh.ARRAY_NORMAL] != null else PackedVector3Array()
	var indices: PackedInt32Array = mesh_data[Mesh.ARRAY_INDEX] if mesh_data[Mesh.ARRAY_INDEX] != null else PackedInt32Array()

	# Build triangle list
	var triangles: Array[PackedInt32Array] = []
	var areas: PackedFloat64Array = PackedFloat64Array()

	if indices.size() > 0:
		for i in range(0, indices.size(), 3):
			var tri := PackedInt32Array([indices[i], indices[i + 1], indices[i + 2]])
			triangles.append(tri)
			areas.append(_triangle_area(vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]))
	else:
		for i in range(0, vertices.size(), 3):
			var tri := PackedInt32Array([i, i + 1, i + 2])
			triangles.append(tri)
			areas.append(_triangle_area(vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]))

	if triangles.is_empty():
		push_warning("SurfaceSampler: mesh has no triangles.")
		return result

	# Build cumulative distribution for area-weighted sampling
	var cumulative: PackedFloat64Array = PackedFloat64Array()
	var total_area: float = 0.0
	for a in areas:
		total_area += a
		cumulative.append(total_area)

	var rng := RandomNumberGenerator.new()
	rng.seed = seed

	for _i in range(point_count):
		# Pick a triangle weighted by area
		var r := rng.randf() * total_area
		var tri_idx := _binary_search(cumulative, r)
		tri_idx = clampi(tri_idx, 0, triangles.size() - 1)

		var tri := triangles[tri_idx]
		var a := vertices[tri[0]]
		var b := vertices[tri[1]]
		var c := vertices[tri[2]]

		# Random barycentric coordinates
		var u := rng.randf()
		var v := rng.randf()
		if u + v > 1.0:
			u = 1.0 - u
			v = 1.0 - v
		var w := 1.0 - u - v

		var pos := a * u + b * v + c * w

		# Interpolate normal if available, otherwise compute face normal
		var nrm: Vector3
		if normals.size() > 0:
			nrm = (normals[tri[0]] * u + normals[tri[1]] * v + normals[tri[2]] * w).normalized()
		else:
			nrm = (b - a).cross(c - a).normalized()

		var point := ScatterPoint.new(pos, nrm)
		result.append(point)

	return result


# -- Helpers ----------------------------------------------------------------

func _get_mesh_arrays() -> Array:
	if mesh_instance == null:
		return []
	var mesh := mesh_instance.mesh
	if mesh == null:
		return []
	if mesh.get_surface_count() == 0:
		return []
	return mesh.surface_get_arrays(0)


func _triangle_area(a: Vector3, b: Vector3, c: Vector3) -> float:
	return (b - a).cross(c - a).length() * 0.5


func _binary_search(cumulative: PackedFloat64Array, value: float) -> int:
	var lo := 0
	var hi := cumulative.size() - 1
	while lo < hi:
		var mid := (lo + hi) / 2
		if cumulative[mid] < value:
			lo = mid + 1
		else:
			hi = mid
	return lo
