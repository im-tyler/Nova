## Spring math utilities for inertialization and smooth decay.
##
## Based on critically-damped spring formulations from Daniel Holden (Ubisoft La Forge)
## and David Bollo (GDC 2016). All springs use halflife parameterization —
## the time in seconds for the value to decay to half its magnitude.
class_name SpringUtils


## Fast approximation of exp(-x). Matches the reference implementation.
## Uses a third-order rational approximation: 1 / (1 + x + 0.48x^2 + 0.235x^3).
static func fast_negexp(x: float) -> float:
	if x < 0.0:
		return 1.0
	return 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x)


## Convert halflife (seconds) to the spring damping coefficient.
## damping = 4 * ln(2) / halflife
static func halflife_to_damping(halflife: float) -> float:
	return (4.0 * 0.69314718056) / maxf(halflife, 1e-5)


## Critically-damped spring that decays a value toward zero.
## Used for inertialization offset decay.
##
## Parameters:
##   x        — current offset value
##   vel      — current velocity
##   halflife — time in seconds for offset to decay to half magnitude
##   dt       — delta time
##
## Returns [new_x, new_vel] as a PackedFloat32Array.
static func decay_spring(x: float, vel: float, halflife: float, dt: float) -> PackedFloat32Array:
	var y := halflife_to_damping(halflife) / 2.0
	var j1 := vel + x * y
	var eydt := fast_negexp(y * dt)
	var new_x := eydt * (x + j1 * dt)
	var new_vel := eydt * (vel - j1 * y * dt)
	return PackedFloat32Array([new_x, new_vel])


## Vector3 version of decay_spring. Decays each component independently.
## Returns [new_position, new_velocity] as a two-element Array of Vector3.
static func decay_spring_vec3(pos: Vector3, vel: Vector3, halflife: float, dt: float) -> Array:
	var rx := decay_spring(pos.x, vel.x, halflife, dt)
	var ry := decay_spring(pos.y, vel.y, halflife, dt)
	var rz := decay_spring(pos.z, vel.z, halflife, dt)
	var new_pos := Vector3(rx[0], ry[0], rz[0])
	var new_vel := Vector3(rx[1], ry[1], rz[1])
	return [new_pos, new_vel]


## Implicit damper spring that moves a value toward a goal position.
## Used for simulation-layer synchronization (pulling character toward target).
##
## Parameters:
##   pos      — current position
##   vel      — current velocity
##   goal     — target position
##   halflife — time for offset to decay to half
##   dt       — delta time
##
## Returns [new_pos, new_vel] as a PackedFloat32Array.
static func damper_spring_implicit(pos: float, vel: float, goal: float, halflife: float, dt: float) -> PackedFloat32Array:
	var y := halflife_to_damping(halflife) / 2.0
	var j0 := pos - goal
	var j1 := vel + j0 * y
	var eydt := fast_negexp(y * dt)
	var new_pos := eydt * (j0 + j1 * dt) + goal
	var new_vel := eydt * (vel - j1 * y * dt)
	return PackedFloat32Array([new_pos, new_vel])


## Vector3 version of damper_spring_implicit.
## Returns [new_position, new_velocity] as a two-element Array of Vector3.
static func damper_spring_implicit_vec3(pos: Vector3, vel: Vector3, goal: Vector3, halflife: float, dt: float) -> Array:
	var rx := damper_spring_implicit(pos.x, vel.x, goal.x, halflife, dt)
	var ry := damper_spring_implicit(pos.y, vel.y, goal.y, halflife, dt)
	var rz := damper_spring_implicit(pos.z, vel.z, goal.z, halflife, dt)
	var new_pos := Vector3(rx[0], ry[0], rz[0])
	var new_vel := Vector3(rx[1], ry[1], rz[1])
	return [new_pos, new_vel]


## Quaternion version of decay spring for rotation offsets.
## Decays a rotation offset (expressed as a quaternion) toward identity.
## The angular velocity is in scaled-axis form (Vector3).
## Returns [new_offset_quat, new_angular_velocity] as a two-element Array.
static func decay_spring_quat(offset: Quaternion, ang_vel: Vector3, halflife: float, dt: float) -> Array:
	# Convert quaternion offset to scaled-axis representation
	var axis_angle := quat_to_scaled_axis(offset)
	var result := decay_spring_vec3(axis_angle, ang_vel, halflife, dt)
	var new_offset := scaled_axis_to_quat(result[0])
	var new_ang_vel: Vector3 = result[1]
	return [new_offset, new_ang_vel]


## Convert a quaternion to scaled-axis (rotation vector) representation.
static func quat_to_scaled_axis(q: Quaternion) -> Vector3:
	# Ensure we take the short path
	var qn := q.normalized()
	if qn.w < 0.0:
		qn = Quaternion(-qn.x, -qn.y, -qn.z, -qn.w)
	var half_angle := acosf(clampf(qn.w, -1.0, 1.0))
	if half_angle < 1e-6:
		return Vector3.ZERO
	var sin_half := sinf(half_angle)
	if absf(sin_half) < 1e-6:
		return Vector3.ZERO
	var axis := Vector3(qn.x, qn.y, qn.z) / sin_half
	return axis * (half_angle * 2.0)


## Convert a scaled-axis (rotation vector) to a quaternion.
static func scaled_axis_to_quat(v: Vector3) -> Quaternion:
	var angle := v.length()
	if angle < 1e-6:
		return Quaternion.IDENTITY
	var axis := v / angle
	var half := angle * 0.5
	var s := sinf(half)
	return Quaternion(axis.x * s, axis.y * s, axis.z * s, cosf(half))
