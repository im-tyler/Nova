#pragma once

// ===========================================================================
// Cascade SPH Fluid Solver — Compute Shaders
//
// Based on:
//   Muller, Charypar, Gross — "Particle-Based Fluid Simulation for
//   Interactive Applications" (2003)
//   Becker, Teschner — "Weakly Compressible SPH for Free Surface Flows" (2007)
//
// SPH kernel: cubic spline (Monaghan 1992)
// Pressure:   Tait equation of state (weakly compressible)
// Viscosity:  XSPH artificial viscosity
//
// Buffer layout:
//   0: positions       vec4(x, y, z, density)
//   1: velocities      vec4(vx, vy, vz, pressure)
//   2: forces          vec4(fx, fy, fz, 0)
//   3: grid_cells      uvec2 (start_index, count) per cell — contiguous ranges
//   4: morton_pairs    uvec2 (morton_code, particle_index) for sorting
//   5: params          uniform block
//   6: sorted_positions  vec4 (reordered positions for cache coherence)
//   7: sorted_velocities vec4 (reordered velocities for cache coherence)
// ===========================================================================

#define FLUID_PARAMS_BLOCK \
"layout(set = 0, binding = 5, std140) uniform FluidParams {\n" \
"    uint num_particles;\n" \
"    float smoothing_radius;\n" \
"    float rest_density;\n" \
"    float gas_constant;\n" \
"    float viscosity;\n" \
"    float surface_tension;\n" \
"    float dt;\n" \
"    float gravity;\n" \
"    float grid_cell_size;\n" \
"    uint grid_width;\n" \
"    uint grid_height;\n" \
"    uint grid_depth;\n" \
"    float bound_min_x;\n" \
"    float bound_min_y;\n" \
"    float bound_min_z;\n" \
"    float bound_max_x;\n" \
"    float bound_max_y;\n" \
"    float bound_max_z;\n" \
"    float time;\n" \
"    float particle_mass;\n" \
"};\n"

// SPH kernel functions (shared by density and force shaders)
#define SPH_KERNELS \
"const float PI = 3.14159265359;\n" \
"\n" \
"// Cubic spline kernel (Monaghan 1992)\n" \
"float W_cubic(float r, float h) {\n" \
"    float q = r / h;\n" \
"    float sigma = 8.0 / (PI * h * h * h);\n" \
"    if (q <= 0.5) {\n" \
"        return sigma * (6.0 * (q*q*q - q*q) + 1.0);\n" \
"    } else if (q <= 1.0) {\n" \
"        float t = 1.0 - q;\n" \
"        return sigma * 2.0 * t * t * t;\n" \
"    }\n" \
"    return 0.0;\n" \
"}\n" \
"\n" \
"// Gradient of cubic spline kernel\n" \
"vec3 grad_W_cubic(vec3 r_vec, float r, float h) {\n" \
"    if (r < 1e-7) return vec3(0.0);\n" \
"    float q = r / h;\n" \
"    float sigma = 8.0 / (PI * h * h * h);\n" \
"    float dWdq;\n" \
"    if (q <= 0.5) {\n" \
"        dWdq = sigma * (18.0 * q * q - 12.0 * q);\n" \
"    } else if (q <= 1.0) {\n" \
"        float t = 1.0 - q;\n" \
"        dWdq = sigma * (-6.0 * t * t);\n" \
"    } else {\n" \
"        return vec3(0.0);\n" \
"    }\n" \
"    return (dWdq / (h * r)) * r_vec;\n" \
"}\n" \
"\n" \
"// Laplacian of viscosity kernel (Muller 2003)\n" \
"float laplacian_W_visc(float r, float h) {\n" \
"    if (r >= h) return 0.0;\n" \
"    return (45.0 / (PI * pow(h, 6.0))) * (h - r);\n" \
"}\n"

// Grid coordinate helpers
#define GRID_HELPERS \
"ivec3 world_to_grid(vec3 pos) {\n" \
"    return ivec3(floor((pos - vec3(bound_min_x, bound_min_y, bound_min_z)) / grid_cell_size));\n" \
"}\n" \
"\n" \
"uint grid_hash(ivec3 cell) {\n" \
"    if (cell.x < 0 || cell.y < 0 || cell.z < 0) return 0xFFFFFFFFu;\n" \
"    if (uint(cell.x) >= grid_width || uint(cell.y) >= grid_height || uint(cell.z) >= grid_depth) return 0xFFFFFFFFu;\n" \
"    return uint(cell.x) + uint(cell.y) * grid_width + uint(cell.z) * grid_width * grid_height;\n" \
"}\n"

// Morton code helpers (bit interleaving for 3D spatial locality)
#define MORTON_HELPERS \
"uint expand_bits(uint v) {\n" \
"    v = (v | (v << 16u)) & 0x030000FFu;\n" \
"    v = (v | (v <<  8u)) & 0x0300F00Fu;\n" \
"    v = (v | (v <<  4u)) & 0x030C30C3u;\n" \
"    v = (v | (v <<  2u)) & 0x09249249u;\n" \
"    return v;\n" \
"}\n" \
"\n" \
"uint morton3d(uint x, uint y, uint z) {\n" \
"    return (expand_bits(x) << 2u) | (expand_bits(y) << 1u) | expand_bits(z);\n" \
"}\n"

// -------------------------------------------------------------------
// CLEAR GRID: reset all cell ranges to (0, 0)
// -------------------------------------------------------------------
static const char *FLUID_CLEAR_GRID_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 3, std430) restrict writeonly buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    uint total = grid_width * grid_height * grid_depth;\n"
"    if (idx >= total) return;\n"
"    grid_cells[idx] = uvec2(0u, 0u);\n"
"}\n"
;

// -------------------------------------------------------------------
// SORT PARTICLES: compute Morton code for each particle from grid position
// -------------------------------------------------------------------
static const char *FLUID_SORT_PARTICLES_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 4, std430) restrict writeonly buffer MortonPairs {\n"
"    uvec2 morton_pairs[];\n"
"};\n"
FLUID_PARAMS_BLOCK
GRID_HELPERS
MORTON_HELPERS
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    ivec3 cell = world_to_grid(positions[idx].xyz);\n"
"    // Clamp to valid grid range for Morton code\n"
"    uint cx = uint(clamp(cell.x, 0, int(grid_width) - 1));\n"
"    uint cy = uint(clamp(cell.y, 0, int(grid_height) - 1));\n"
"    uint cz = uint(clamp(cell.z, 0, int(grid_depth) - 1));\n"
"\n"
"    uint code = morton3d(cx, cy, cz);\n"
"    morton_pairs[idx] = uvec2(code, idx);\n"
"}\n"
;

// -------------------------------------------------------------------
// BITONIC SORT: GPU-friendly parallel sort on Morton code pairs
// Uses push constants for step/substep parameters
// -------------------------------------------------------------------
static const char *FLUID_BITONIC_SORT_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 4, std430) restrict buffer MortonPairs {\n"
"    uvec2 morton_pairs[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"layout(push_constant) uniform PushConstants {\n"
"    uint k; // major step (block size)\n"
"    uint j; // substep\n"
"    uint pad0;\n"
"    uint pad1;\n"
"} pc;\n"
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    // Bitonic merge compare-and-swap\n"
"    uint l = idx ^ pc.j;\n"
"    if (l <= idx) return;\n"
"    if (idx >= num_particles || l >= num_particles) return;\n"
"\n"
"    uvec2 a = morton_pairs[idx];\n"
"    uvec2 b = morton_pairs[l];\n"
"\n"
"    bool ascending = ((idx & pc.k) == 0u);\n"
"    bool swap = ascending ? (a.x > b.x) : (a.x < b.x);\n"
"\n"
"    if (swap) {\n"
"        morton_pairs[idx] = b;\n"
"        morton_pairs[l] = a;\n"
"    }\n"
"}\n"
;

// -------------------------------------------------------------------
// REORDER PARTICLES: copy particle data into sorted order
// -------------------------------------------------------------------
static const char *FLUID_REORDER_PARTICLES_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict readonly buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 4, std430) restrict readonly buffer MortonPairs {\n"
"    uvec2 morton_pairs[];\n"
"};\n"
"layout(set = 0, binding = 6, std430) restrict writeonly buffer SortedPositions {\n"
"    vec4 sorted_positions[];\n"
"};\n"
"layout(set = 0, binding = 7, std430) restrict writeonly buffer SortedVelocities {\n"
"    vec4 sorted_velocities[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    uint src = morton_pairs[idx].y;\n"
"    sorted_positions[idx] = positions[src];\n"
"    sorted_velocities[idx] = velocities[src];\n"
"}\n"
;

// -------------------------------------------------------------------
// COPY BACK: copy sorted data back to primary buffers
// -------------------------------------------------------------------
static const char *FLUID_COPY_BACK_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict writeonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict writeonly buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 6, std430) restrict readonly buffer SortedPositions {\n"
"    vec4 sorted_positions[];\n"
"};\n"
"layout(set = 0, binding = 7, std430) restrict readonly buffer SortedVelocities {\n"
"    vec4 sorted_velocities[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    positions[idx] = sorted_positions[idx];\n"
"    velocities[idx] = sorted_velocities[idx];\n"
"}\n"
;

// -------------------------------------------------------------------
// BUILD GRID: count particles per cell and compute contiguous ranges
// -------------------------------------------------------------------
static const char *FLUID_BUILD_GRID_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 3, std430) restrict buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
FLUID_PARAMS_BLOCK
GRID_HELPERS
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    ivec3 cell = world_to_grid(positions[idx].xyz);\n"
"    uint hash = grid_hash(cell);\n"
"    if (hash == 0xFFFFFFFFu) return;\n"
"\n"
"    // Atomically increment count for this cell\n"
"    // grid_cells[hash].x = start (computed in second pass), .y = count\n"
"    atomicAdd(grid_cells[hash].y, 1u);\n"
"}\n"
;

// -------------------------------------------------------------------
// PREFIX SUM: compute start offsets from counts (serial, run on 1 thread)
// For small grids this is fine; for large grids a parallel prefix sum
// would be needed but the grid is typically small (< 100k cells).
// -------------------------------------------------------------------
static const char *FLUID_PREFIX_SUM_SHADER =
"#version 450\n"
"layout(local_size_x = 1) in;\n"
"\n"
"layout(set = 0, binding = 3, std430) restrict buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint total = grid_width * grid_height * grid_depth;\n"
"    uint running = 0u;\n"
"    for (uint i = 0u; i < total; i++) {\n"
"        uint count = grid_cells[i].y;\n"
"        grid_cells[i].x = running;\n"
"        running += count;\n"
"    }\n"
"}\n"
;

// -------------------------------------------------------------------
// CLEAR COUNTERS: zero out the per-cell atomic counters before scatter
// -------------------------------------------------------------------
static const char *FLUID_CLEAR_COUNTERS_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 8, std430) restrict writeonly buffer CellCounters {\n"
"    uint cell_counters[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    uint total = grid_width * grid_height * grid_depth;\n"
"    if (idx >= total) return;\n"
"    cell_counters[idx] = 0u;\n"
"}\n"
;

// -------------------------------------------------------------------
// SCATTER: place each particle index into contiguous grid cell ranges
// Uses an atomic counter per cell to assign slots
// Needs a separate write counter buffer (binding 8)
// -------------------------------------------------------------------
static const char *FLUID_SCATTER_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 3, std430) restrict readonly buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
"layout(set = 0, binding = 4, std430) restrict writeonly buffer CellIndices {\n"
"    uint cell_indices[];\n"
"};\n"
"layout(set = 0, binding = 8, std430) restrict buffer CellCounters {\n"
"    uint cell_counters[];\n"
"};\n"
FLUID_PARAMS_BLOCK
GRID_HELPERS
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    ivec3 cell = world_to_grid(positions[idx].xyz);\n"
"    uint hash = grid_hash(cell);\n"
"    if (hash == 0xFFFFFFFFu) return;\n"
"\n"
"    uint offset = atomicAdd(cell_counters[hash], 1u);\n"
"    cell_indices[grid_cells[hash].x + offset] = idx;\n"
"}\n"
;

// -------------------------------------------------------------------
// DENSITY: compute density and pressure for each particle
// Uses contiguous-range grid cells for cache-coherent neighbor search
// -------------------------------------------------------------------
static const char *FLUID_DENSITY_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 3, std430) restrict readonly buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
"layout(set = 0, binding = 4, std430) restrict readonly buffer CellIndices {\n"
"    uint cell_indices[];\n"
"};\n"
FLUID_PARAMS_BLOCK
SPH_KERNELS
GRID_HELPERS
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    vec3 pos_i = positions[idx].xyz;\n"
"    ivec3 cell_i = world_to_grid(pos_i);\n"
"\n"
"    float density = 0.0;\n"
"\n"
"    // Search 3x3x3 neighborhood\n"
"    for (int dz = -1; dz <= 1; dz++) {\n"
"    for (int dy = -1; dy <= 1; dy++) {\n"
"    for (int dx = -1; dx <= 1; dx++) {\n"
"        ivec3 neighbor_cell = cell_i + ivec3(dx, dy, dz);\n"
"        uint hash = grid_hash(neighbor_cell);\n"
"        if (hash == 0xFFFFFFFFu) continue;\n"
"\n"
"        uint start = grid_cells[hash].x;\n"
"        uint count = grid_cells[hash].y;\n"
"        for (uint k = 0u; k < count; k++) {\n"
"            uint j = cell_indices[start + k];\n"
"            vec3 pos_j = positions[j].xyz;\n"
"            float r = length(pos_i - pos_j);\n"
"            density += particle_mass * W_cubic(r, smoothing_radius);\n"
"        }\n"
"    }}}\n"
"\n"
"    // Tait equation of state (weakly compressible)\n"
"    float pressure = gas_constant * (density - rest_density);\n"
"\n"
"    positions[idx].w = density;\n"
"    velocities[idx].w = pressure;\n"
"}\n"
;

// -------------------------------------------------------------------
// FORCES: compute pressure, viscosity, and external forces
// Uses contiguous-range grid cells for cache-coherent neighbor search
// -------------------------------------------------------------------
static const char *FLUID_FORCES_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict readonly buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict readonly buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 2, std430) restrict writeonly buffer Forces {\n"
"    vec4 forces[];\n"
"};\n"
"layout(set = 0, binding = 3, std430) restrict readonly buffer GridCells {\n"
"    uvec2 grid_cells[];\n"
"};\n"
"layout(set = 0, binding = 4, std430) restrict readonly buffer CellIndices {\n"
"    uint cell_indices[];\n"
"};\n"
FLUID_PARAMS_BLOCK
SPH_KERNELS
GRID_HELPERS
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    vec3 pos_i = positions[idx].xyz;\n"
"    vec3 vel_i = velocities[idx].xyz;\n"
"    float density_i = positions[idx].w;\n"
"    float pressure_i = velocities[idx].w;\n"
"\n"
"    if (density_i < 1e-7) {\n"
"        forces[idx] = vec4(0.0, gravity, 0.0, 0.0);\n"
"        return;\n"
"    }\n"
"\n"
"    ivec3 cell_i = world_to_grid(pos_i);\n"
"    vec3 f_pressure = vec3(0.0);\n"
"    vec3 f_viscosity = vec3(0.0);\n"
"\n"
"    for (int dz = -1; dz <= 1; dz++) {\n"
"    for (int dy = -1; dy <= 1; dy++) {\n"
"    for (int dx = -1; dx <= 1; dx++) {\n"
"        ivec3 neighbor_cell = cell_i + ivec3(dx, dy, dz);\n"
"        uint hash = grid_hash(neighbor_cell);\n"
"        if (hash == 0xFFFFFFFFu) continue;\n"
"\n"
"        uint start = grid_cells[hash].x;\n"
"        uint count = grid_cells[hash].y;\n"
"        for (uint k = 0u; k < count; k++) {\n"
"            uint j = cell_indices[start + k];\n"
"            if (j != idx) {\n"
"                vec3 pos_j = positions[j].xyz;\n"
"                vec3 vel_j = velocities[j].xyz;\n"
"                float density_j = positions[j].w;\n"
"                float pressure_j = velocities[j].w;\n"
"\n"
"                vec3 r_vec = pos_i - pos_j;\n"
"                float r = length(r_vec);\n"
"\n"
"                if (r < smoothing_radius && density_j > 1e-7) {\n"
"                    // Symmetric pressure force (Muller 2003)\n"
"                    vec3 gradW = grad_W_cubic(r_vec, r, smoothing_radius);\n"
"                    f_pressure -= particle_mass * (pressure_i / (density_i * density_i) +\n"
"                                                   pressure_j / (density_j * density_j)) * gradW;\n"
"\n"
"                    // Viscosity (Muller 2003)\n"
"                    float lapW = laplacian_W_visc(r, smoothing_radius);\n"
"                    f_viscosity += viscosity * particle_mass *\n"
"                                  (vel_j - vel_i) / density_j * lapW;\n"
"                }\n"
"            }\n"
"        }\n"
"    }}}\n"
"\n"
"    vec3 f_total = f_pressure + f_viscosity;\n"
"\n"
"    // Gravity\n"
"    f_total.y += gravity;\n"
"\n"
"    forces[idx] = vec4(f_total, 0.0);\n"
"}\n"
;

// -------------------------------------------------------------------
// INTEGRATE: update velocities and positions, enforce boundaries
// -------------------------------------------------------------------
static const char *FLUID_INTEGRATE_SHADER =
"#version 450\n"
"layout(local_size_x = 64) in;\n"
"\n"
"layout(set = 0, binding = 0, std430) restrict buffer Positions {\n"
"    vec4 positions[];\n"
"};\n"
"layout(set = 0, binding = 1, std430) restrict buffer Velocities {\n"
"    vec4 velocities[];\n"
"};\n"
"layout(set = 0, binding = 2, std430) restrict readonly buffer Forces {\n"
"    vec4 forces[];\n"
"};\n"
FLUID_PARAMS_BLOCK
"\n"
"void main() {\n"
"    uint idx = gl_GlobalInvocationID.x;\n"
"    if (idx >= num_particles) return;\n"
"\n"
"    vec3 pos = positions[idx].xyz;\n"
"    vec3 vel = velocities[idx].xyz;\n"
"    float density = positions[idx].w;\n"
"    float pressure = velocities[idx].w;\n"
"\n"
"    vec3 accel = forces[idx].xyz;\n"
"\n"
"    // Symplectic Euler\n"
"    vel += accel * dt;\n"
"    pos += vel * dt;\n"
"\n"
"    // Boundary enforcement with damping\n"
"    float damping = 0.3;\n"
"    float eps = 0.001;\n"
"\n"
"    if (pos.x < bound_min_x + eps) { pos.x = bound_min_x + eps; vel.x *= -damping; }\n"
"    if (pos.x > bound_max_x - eps) { pos.x = bound_max_x - eps; vel.x *= -damping; }\n"
"    if (pos.y < bound_min_y + eps) { pos.y = bound_min_y + eps; vel.y *= -damping; }\n"
"    if (pos.y > bound_max_y - eps) { pos.y = bound_max_y - eps; vel.y *= -damping; }\n"
"    if (pos.z < bound_min_z + eps) { pos.z = bound_min_z + eps; vel.z *= -damping; }\n"
"    if (pos.z > bound_max_z - eps) { pos.z = bound_max_z - eps; vel.z *= -damping; }\n"
"\n"
"    positions[idx] = vec4(pos, density);\n"
"    velocities[idx] = vec4(vel, pressure);\n"
"}\n"
;
