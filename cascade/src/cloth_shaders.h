#pragma once

// ===========================================================================
// Cascade XPBD Cloth Solver — Compute Shaders
//
// Based on:
//   Macklin, Muller, Chentanez — "XPBD: Position-Based Simulation of
//   Compliant Constrained Dynamics" (2016)
//
// Key properties:
//   - Lagrange multiplier accumulation for timestep independence
//   - Graph-colored parallel constraint solving (Gauss-Seidel)
//   - Compliance-based stiffness (physically meaningful units)
//   - Area-weighted vertex normals
//
// Buffer layout:
//   0: positions   vec4(x, y, z, inv_mass)
//   1: predicted   vec4(px, py, pz, 0)
//   2: velocities  vec4(vx, vy, vz, 0)
//   3: constraints uvec4(idx_a, idx_b, rest_length_bits, compliance_bits)
//   4: params      uniform block
//   5: normals     vec4(nx, ny, nz, 0)
//   6: colliders   vec4(data per collider)
//   7: lambdas     float (one per constraint, Lagrange multipliers)
// ===========================================================================

// Shared params block used by all shaders
#define PARAMS_BLOCK \
"layout(set = 0, binding = 4, std140) uniform Params {\n" \
"    uint num_vertices;\n" \
"    uint num_constraints;\n" \
"    uint constraint_offset;\n" \
"    uint constraint_count;\n" \
"    float dt;\n" \
"    float gravity;\n" \
"    float damping;\n" \
"    uint num_colliders;\n" \
"    uint grid_width;\n" \
"    uint grid_height;\n" \
"    uint num_sphere_colliders;\n" \
"    uint num_plane_colliders;\n" \
"    float wind_x;\n" \
"    float wind_y;\n" \
"    float wind_z;\n" \
"    float wind_turbulence;\n" \
"    float time;\n" \
"    float friction;\n" \
"    float cloth_thickness;\n" \
"    uint self_collision_grid_size;\n" \
"};\n"

// -------------------------------------------------------------------
// PREDICT: apply external forces, compute predicted positions,
//          reset Lagrange multipliers for this frame
// -------------------------------------------------------------------
static const char *CLOTH_PREDICT_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict writeonly buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 2, std430) restrict buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 7, std430) restrict writeonly buffer Lambdas {\n"
"    float lambdas[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"// Simple 3D hash for wind turbulence\n"
"float hash31(vec3 p) {\n"
"    p = fract(p * vec3(0.1031, 0.1030, 0.0973));\n"
"    p += dot(p, p.yzx + 33.33);\n"
"    return fract((p.x + p.y) * p.z);\n"
"}\n"
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    vec4 pos = positions[idx];\n"
"    vec4 vel = velocities[idx];\n"
"    float inv_mass = pos.w;\n"
"\n"
"    // Pinned vertex\n"
"    if (inv_mass <= 0.0) {\n"
"        predicted[idx] = vec4(pos.xyz, 0.0);\n"
"        return;\n"
"    }\n"
"\n"
"    // External forces\n"
"    vec3 f_ext = vec3(0.0, gravity, 0.0);\n"
"\n"
"    // Wind with turbulence\n"
"    vec3 wind = vec3(wind_x, wind_y, wind_z);\n"
"    if (wind_turbulence > 0.0) {\n"
"        float noise = hash31(pos.xyz * 2.0 + vec3(time)) * 2.0 - 1.0;\n"
"        wind += wind * noise * wind_turbulence;\n"
"    }\n"
"    f_ext += wind * inv_mass;\n"
"\n"
"    // Symplectic Euler integration\n"
"    vec3 v = vel.xyz + dt * f_ext;\n"
"    vec3 p = pos.xyz + dt * v;\n"
"\n"
"    velocities[idx] = vec4(v, 0.0);\n"
"    predicted[idx] = vec4(p, 0.0);\n"
"}\n"
"\n"
"// Reset lambdas in a separate dispatch over num_constraints\n"
;

static const char *CLOTH_RESET_LAMBDAS_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 7, std430) restrict writeonly buffer Lambdas {\n"
"    float lambdas[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_constraints) return;\n"
"    lambdas[idx] = 0.0;\n"
"}\n"
;

// -------------------------------------------------------------------
// SOLVE CONSTRAINTS: XPBD with Lagrange multiplier accumulation
//
// For each constraint j:
//   C_j = |p_b - p_a| - rest_length
//   alpha_tilde = compliance / dt^2
//   delta_lambda = (-C_j - alpha_tilde * lambda_j) / (w_a + w_b + alpha_tilde)
//   lambda_j += delta_lambda
//   dp = delta_lambda * normalize(p_b - p_a)
//   p_a -= w_a * dp
//   p_b += w_b * dp
// -------------------------------------------------------------------
static const char *CLOTH_SOLVE_CONSTRAINTS_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 3, std430) restrict readonly buffer Constraints {\n"
"    uvec4 constraints[];\n"
"};\n"
"layout(set = 0, binding = 7, std430) restrict buffer Lambdas {\n"
"    float lambdas[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint local_idx = gl_GlobalInvocationID.x;\n"
"    if (local_idx >= constraint_count) return;\n"
"\n"
"    uint global_idx = constraint_offset + local_idx;\n"
"    uvec4 c = constraints[global_idx];\n"
"    uint idx_a = c.x;\n"
"    uint idx_b = c.y;\n"
"    float rest_length = uintBitsToFloat(c.z);\n"
"    float compliance = uintBitsToFloat(c.w);\n"
"\n"
"    vec3 p_a = predicted[idx_a].xyz;\n"
"    vec3 p_b = predicted[idx_b].xyz;\n"
"    float w_a = positions[idx_a].w;\n"
"    float w_b = positions[idx_b].w;\n"
"\n"
"    float w_sum = w_a + w_b;\n"
"    if (w_sum < 1e-7) return;\n"
"\n"
"    vec3 diff = p_b - p_a;\n"
"    float dist = length(diff);\n"
"    if (dist < 1e-7) return;\n"
"\n"
"    vec3 grad = diff / dist;\n"
"\n"
"    // XPBD: compliance scaled by timestep squared\n"
"    float alpha_tilde = compliance / (dt * dt);\n"
"\n"
"    // Constraint value\n"
"    float C = dist - rest_length;\n"
"\n"
"    // Lagrange multiplier update\n"
"    float lambda = lambdas[global_idx];\n"
"    float delta_lambda = (-C - alpha_tilde * lambda) / (w_sum + alpha_tilde);\n"
"    lambdas[global_idx] = lambda + delta_lambda;\n"
"\n"
"    // Position corrections\n"
"    vec3 correction = delta_lambda * grad;\n"
"\n"
"    if (w_a > 0.0)\n"
"        predicted[idx_a] = vec4(p_a - w_a * correction, 0.0);\n"
"    if (w_b > 0.0)\n"
"        predicted[idx_b] = vec4(p_b + w_b * correction, 0.0);\n"
"}\n"
;

// -------------------------------------------------------------------
// COLLIDE: push predicted positions out of colliders
//          Spheres: vec4(cx, cy, cz, radius)
//          Planes:  vec4(nx, ny, nz, d)  where dot(p,n)+d >= 0 is outside
// -------------------------------------------------------------------
static const char *CLOTH_COLLIDE_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 6, std430) restrict readonly buffer Colliders {\n"
"    vec4 colliders[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    float inv_mass = positions[idx].w;\n"
"    if (inv_mass <= 0.0) return;\n"
"\n"
"    vec3 p = predicted[idx].xyz;\n"
"    vec3 old_p = p;\n"
"    uint offset = 0u;\n"
"\n"
"    // Sphere colliders\n"
"    for (uint i = 0u; i < num_sphere_colliders; i++) {\n"
"        vec4 sphere = colliders[offset + i];\n"
"        vec3 center = sphere.xyz;\n"
"        float radius = sphere.w;\n"
"        vec3 diff = p - center;\n"
"        float dist = length(diff);\n"
"        if (dist < radius && dist > 1e-7) {\n"
"            vec3 n = diff / dist;\n"
"            p = center + n * radius;\n"
"            // Friction: project tangential displacement\n"
"            if (friction > 0.0) {\n"
"                vec3 dp = p - old_p;\n"
"                vec3 dp_n = dot(dp, n) * n;\n"
"                vec3 dp_t = dp - dp_n;\n"
"                float dp_t_len = length(dp_t);\n"
"                if (dp_t_len > 1e-7) {\n"
"                    float max_friction = friction * length(dp_n);\n"
"                    dp_t *= max(0.0, 1.0 - max_friction / dp_t_len);\n"
"                }\n"
"                p = old_p + dp_n + dp_t;\n"
"            }\n"
"        }\n"
"    }\n"
"    offset += num_sphere_colliders;\n"
"\n"
"    // Plane colliders\n"
"    for (uint i = 0u; i < num_plane_colliders; i++) {\n"
"        vec4 plane = colliders[offset + i];\n"
"        vec3 n = plane.xyz;\n"
"        float d = plane.w;\n"
"        float penetration = dot(p, n) + d;\n"
"        if (penetration < 0.0) {\n"
"            p -= n * penetration;\n"
"            // Friction\n"
"            if (friction > 0.0) {\n"
"                vec3 dp = p - old_p;\n"
"                vec3 dp_n = dot(dp, n) * n;\n"
"                vec3 dp_t = dp - dp_n;\n"
"                float dp_t_len = length(dp_t);\n"
"                if (dp_t_len > 1e-7) {\n"
"                    float max_friction = friction * length(dp_n);\n"
"                    dp_t *= max(0.0, 1.0 - max_friction / dp_t_len);\n"
"                }\n"
"                p = old_p + dp_n + dp_t;\n"
"            }\n"
"        }\n"
"    }\n"
"\n"
"    predicted[idx] = vec4(p, 0.0);\n"
"}\n"
;

// -------------------------------------------------------------------
// UPDATE: compute velocities from position delta, update positions
//         v = (p - x) / dt   (no explicit damping — XPBD compliance handles it)
// -------------------------------------------------------------------
static const char *CLOTH_UPDATE_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict readonly buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 2, std430) restrict writeonly buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    float inv_mass = positions[idx].w;\n"
"    if (inv_mass <= 0.0) return;\n"
"\n"
"    vec3 x_old = positions[idx].xyz;\n"
"    vec3 x_new = predicted[idx].xyz;\n"
"\n"
"    // Velocity from position change (XPBD Eq. 5)\n"
"    vec3 v = (x_new - x_old) / dt;\n"
"\n"
"    // Velocity damping (small amount for stability)\n"
"    v *= damping;\n"
"\n"
"    velocities[idx] = vec4(v, 0.0);\n"
"    positions[idx] = vec4(x_new, inv_mass);\n"
"}\n"
;

// -------------------------------------------------------------------
// NORMALS: area-weighted vertex normals from adjacent triangles
// -------------------------------------------------------------------
static const char *CLOTH_NORMALS_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 5, std430) restrict writeonly buffer Normals {\n"
"    vec4 normals[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    uint row = idx / grid_width;\n"
"    uint col = idx % grid_width;\n"
"    vec3 pos = positions[idx].xyz;\n"
"\n"
"    // Area-weighted normal: sum cross products of all adjacent triangle edges\n"
"    // The magnitude of the cross product IS the area weight, so no normalization\n"
"    // of individual contributions needed — just normalize the sum.\n"
"    vec3 normal = vec3(0.0);\n"
"\n"
"    // Get neighbor positions (clamped to grid bounds)\n"
"    bool has_left  = col > 0u;\n"
"    bool has_right = col < grid_width - 1u;\n"
"    bool has_up    = row > 0u;\n"
"    bool has_down  = row < grid_height - 1u;\n"
"\n"
"    vec3 left  = has_left  ? positions[idx - 1u].xyz - pos : vec3(0.0);\n"
"    vec3 right = has_right ? positions[idx + 1u].xyz - pos : vec3(0.0);\n"
"    vec3 up    = has_up    ? positions[idx - grid_width].xyz - pos : vec3(0.0);\n"
"    vec3 down  = has_down  ? positions[idx + grid_width].xyz - pos : vec3(0.0);\n"
"\n"
"    // Sum cross products of adjacent edge pairs (CCW winding)\n"
"    if (has_right && has_down) normal += cross(right, down);\n"
"    if (has_down && has_left)  normal += cross(down, left);\n"
"    if (has_left && has_up)    normal += cross(left, up);\n"
"    if (has_up && has_right)   normal += cross(up, right);\n"
"\n"
"    float len = length(normal);\n"
"    normal = len > 1e-7 ? normal / len : vec3(0.0, 0.0, 1.0);\n"
"\n"
"    normals[idx] = vec4(normal, 0.0);\n"
"}\n"
;

// -------------------------------------------------------------------
// SELF-COLLISION: spatial hash grid to detect and resolve
// cloth-on-cloth penetration
//
// Buffer layout for self-collision:
//   8: sc_grid_heads  uint (linked list head per cell, 0xFFFFFFFF = empty)
//   9: sc_grid_next   uint (per-vertex next pointer in cell list)
// -------------------------------------------------------------------

static const char *CLOTH_SC_CLEAR_GRID_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 8, std430) restrict writeonly buffer GridHeads {\n"
"    uint grid_heads[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    uint total = self_collision_grid_size * self_collision_grid_size * self_collision_grid_size;\n"
"    if (idx >= total) return;\n"
"    grid_heads[idx] = 0xFFFFFFFFu;\n"
"}\n"
;

static const char *CLOTH_SC_BUILD_GRID_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 1, std430) restrict readonly buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 8, std430) restrict buffer GridHeads {\n"
"    uint grid_heads[];\n"
"};\n"
"layout(set = 0, binding = 9, std430) restrict writeonly buffer GridNext {\n"
"    uint grid_next[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"uint hash_cell(ivec3 cell) {\n"
"    uint gs = self_collision_grid_size;\n"
"    if (cell.x < 0 || cell.y < 0 || cell.z < 0) return 0xFFFFFFFFu;\n"
"    if (uint(cell.x) >= gs || uint(cell.y) >= gs || uint(cell.z) >= gs) return 0xFFFFFFFFu;\n"
"    return uint(cell.x) + uint(cell.y) * gs + uint(cell.z) * gs * gs;\n"
"}\n"
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    vec3 p = predicted[idx].xyz;\n"
"    float cell_size = cloth_thickness * 2.0;\n"
"    ivec3 cell = ivec3(floor(p / cell_size)) + ivec3(int(self_collision_grid_size) / 2);\n"
"    uint h = hash_cell(cell);\n"
"    if (h == 0xFFFFFFFFu) {\n"
"        grid_next[idx] = 0xFFFFFFFFu;\n"
"        return;\n"
"    }\n"
"    uint old = atomicExchange(grid_heads[h], idx);\n"
"    grid_next[idx] = old;\n"
"}\n"
;

static const char *CLOTH_SC_RESOLVE_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict buffer Predicted {\n"
"    vec4 predicted[];\n"
"};\n"
"layout(set = 0, binding = 8, std430) restrict readonly buffer GridHeads {\n"
"    uint grid_heads[];\n"
"};\n"
"layout(set = 0, binding = 9, std430) restrict readonly buffer GridNext {\n"
"    uint grid_next[];\n"
"};\n"
PARAMS_BLOCK
"\n"
"uint hash_cell(ivec3 cell) {\n"
"    uint gs = self_collision_grid_size;\n"
"    if (cell.x < 0 || cell.y < 0 || cell.z < 0) return 0xFFFFFFFFu;\n"
"    if (uint(cell.x) >= gs || uint(cell.y) >= gs || uint(cell.z) >= gs) return 0xFFFFFFFFu;\n"
"    return uint(cell.x) + uint(cell.y) * gs + uint(cell.z) * gs * gs;\n"
"}\n"
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_vertices) return;\n"
"\n"
"    float inv_mass = positions[idx].w;\n"
"    if (inv_mass <= 0.0) return;\n"
"\n"
"    vec3 p = predicted[idx].xyz;\n"
"    float cell_size = cloth_thickness * 2.0;\n"
"    ivec3 my_cell = ivec3(floor(p / cell_size)) + ivec3(int(self_collision_grid_size) / 2);\n"
"\n"
"    vec3 correction = vec3(0.0);\n"
"    int num_corrections = 0;\n"
"\n"
"    // Search 3x3x3 neighborhood\n"
"    for (int dz = -1; dz <= 1; dz++) {\n"
"    for (int dy = -1; dy <= 1; dy++) {\n"
"    for (int dx = -1; dx <= 1; dx++) {\n"
"        ivec3 cell = my_cell + ivec3(dx, dy, dz);\n"
"        uint h = hash_cell(cell);\n"
"        if (h == 0xFFFFFFFFu) continue;\n"
"\n"
"        uint j = grid_heads[h];\n"
"        while (j != 0xFFFFFFFFu) {\n"
"            if (j != idx) {\n"
"                // Skip topologically adjacent vertices (within 2 hops in grid)\n"
"                // Simple approximation: skip if index difference < grid_width*2\n"
"                int idx_diff = abs(int(j) - int(idx));\n"
"                if (idx_diff > 2 && idx_diff != int(grid_width) && idx_diff != int(grid_width) + 1 && idx_diff != int(grid_width) - 1) {\n"
"                    vec3 pj = predicted[j].xyz;\n"
"                    vec3 diff = p - pj;\n"
"                    float dist = length(diff);\n"
"\n"
"                    if (dist < cloth_thickness && dist > 1e-7) {\n"
"                        // Push apart to thickness distance\n"
"                        vec3 dir = diff / dist;\n"
"                        float overlap = cloth_thickness - dist;\n"
"                        correction += dir * overlap * 0.5;\n"
"                        num_corrections++;\n"
"                    }\n"
"                }\n"
"            }\n"
"            j = grid_next[j];\n"
"        }\n"
"    }}}\n"
"\n"
"    if (num_corrections > 0) {\n"
"        predicted[idx] = vec4(p + correction / float(num_corrections), 0.0);\n"
"    }\n"
"}\n"
;
