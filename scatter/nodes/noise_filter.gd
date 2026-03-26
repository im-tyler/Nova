## Filters points using a hash-based noise function.
##
## Points where the noise value exceeds the threshold are kept; the rest are
## discarded. Useful for breaking up uniform distributions into organic clumps.
@tool
class_name NoiseFilter
extends ScatterNode

## Scale of the noise sampling. Larger values = broader features.
@export var noise_scale: float = 1.0

## Noise threshold. Points with noise value > threshold are kept.
@export_range(0.0, 1.0, 0.01) var threshold: float = 0.5

## Seed for the noise function.
@export var seed: int = 0


func _init() -> void:
	label = "Noise Filter"


func get_class_name_custom() -> String:
	return "NoiseFilter"


func execute(points: Array[ScatterPoint]) -> Array[ScatterPoint]:
	var result: Array[ScatterPoint] = []

	for point in points:
		var noise_val := _hash_noise_3d(point.position, noise_scale, seed)
		if noise_val > threshold:
			result.append(point)

	return result


## Simple hash-based 3D noise function that returns a value in [0, 1].
## Not true Perlin/simplex, but a fast deterministic alternative suitable
## for point filtering where smoothness between adjacent samples is not
## critical.
static func _hash_noise_3d(pos: Vector3, scale: float, p_seed: int) -> float:
	var scaled := pos * scale
	# Use three large primes to hash the position components together with
	# the seed. This gives a pseudo-random but deterministic value per point.
	var ix := int(floor(scaled.x))
	var iy := int(floor(scaled.y))
	var iz := int(floor(scaled.z))

	# Fractional parts for interpolation.
	var fx := scaled.x - floor(scaled.x)
	var fy := scaled.y - floor(scaled.y)
	var fz := scaled.z - floor(scaled.z)

	# Smooth interpolation curves.
	fx = fx * fx * (3.0 - 2.0 * fx)
	fy = fy * fy * (3.0 - 2.0 * fy)
	fz = fz * fz * (3.0 - 2.0 * fz)

	# Hash the eight corners of the unit cube and trilinearly interpolate.
	var c000 := _hash_int3(ix,     iy,     iz,     p_seed)
	var c100 := _hash_int3(ix + 1, iy,     iz,     p_seed)
	var c010 := _hash_int3(ix,     iy + 1, iz,     p_seed)
	var c110 := _hash_int3(ix + 1, iy + 1, iz,     p_seed)
	var c001 := _hash_int3(ix,     iy,     iz + 1, p_seed)
	var c101 := _hash_int3(ix + 1, iy,     iz + 1, p_seed)
	var c011 := _hash_int3(ix,     iy + 1, iz + 1, p_seed)
	var c111 := _hash_int3(ix + 1, iy + 1, iz + 1, p_seed)

	var x00 := lerpf(c000, c100, fx)
	var x10 := lerpf(c010, c110, fx)
	var x01 := lerpf(c001, c101, fx)
	var x11 := lerpf(c011, c111, fx)

	var y0 := lerpf(x00, x10, fy)
	var y1 := lerpf(x01, x11, fy)

	return lerpf(y0, y1, fz)


## Integer hash returning a float in [0, 1]. Uses bit mixing with large primes.
static func _hash_int3(x: int, y: int, z: int, p_seed: int) -> float:
	var h := p_seed
	h = h ^ (x * 374761393)
	h = h ^ (y * 668265263)
	h = h ^ (z * 1274126177)
	h = h ^ (h >> 13)
	h = h * 1103515245
	h = h ^ (h >> 16)
	# Map to [0, 1].
	return float(abs(h) % 100000) / 100000.0
