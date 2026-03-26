// ===========================================================================
// Cascade SPH Fluid Solver
//
// GPU-accelerated Smoothed Particle Hydrodynamics using spatial hashing
// for neighbor search. Renders via MultiMesh instanced spheres.
//
// References:
//   Muller, Charypar, Gross (2003) — SPH for interactive applications
//   Becker, Teschner (2007) — Weakly Compressible SPH
//   PhysX 5.6 particle system (architecture reference, BSD-3)
// ===========================================================================

#include "cascade_fluid.h"
#include "fluid_shaders.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <cmath>
#include <vector>

using namespace godot;

CascadeFluid::CascadeFluid() {}
CascadeFluid::~CascadeFluid() { _cleanup_gpu(); }

void CascadeFluid::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_num_particles", "n"), &CascadeFluid::set_num_particles);
    ClassDB::bind_method(D_METHOD("get_num_particles"), &CascadeFluid::get_num_particles);
    ClassDB::bind_method(D_METHOD("set_simulate", "enable"), &CascadeFluid::set_simulate);
    ClassDB::bind_method(D_METHOD("get_simulate"), &CascadeFluid::get_simulate);
    ClassDB::bind_method(D_METHOD("set_smoothing_radius", "r"), &CascadeFluid::set_smoothing_radius);
    ClassDB::bind_method(D_METHOD("get_smoothing_radius"), &CascadeFluid::get_smoothing_radius);
    ClassDB::bind_method(D_METHOD("set_rest_density", "d"), &CascadeFluid::set_rest_density);
    ClassDB::bind_method(D_METHOD("get_rest_density"), &CascadeFluid::get_rest_density);
    ClassDB::bind_method(D_METHOD("set_viscosity_coeff", "v"), &CascadeFluid::set_viscosity_coeff);
    ClassDB::bind_method(D_METHOD("get_viscosity_coeff"), &CascadeFluid::get_viscosity_coeff);
    ClassDB::bind_method(D_METHOD("set_particle_radius", "r"), &CascadeFluid::set_particle_radius);
    ClassDB::bind_method(D_METHOD("get_particle_radius"), &CascadeFluid::get_particle_radius);
    ClassDB::bind_method(D_METHOD("set_bounds_min", "v"), &CascadeFluid::set_bounds_min);
    ClassDB::bind_method(D_METHOD("get_bounds_min"), &CascadeFluid::get_bounds_min);
    ClassDB::bind_method(D_METHOD("set_bounds_max", "v"), &CascadeFluid::set_bounds_max);
    ClassDB::bind_method(D_METHOD("get_bounds_max"), &CascadeFluid::get_bounds_max);

    ADD_GROUP("Fluid", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_particles", PROPERTY_HINT_RANGE, "64,65536,64"), "set_num_particles", "get_num_particles");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "smoothing_radius", PROPERTY_HINT_RANGE, "0.01,1.0,0.01"), "set_smoothing_radius", "get_smoothing_radius");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rest_density", PROPERTY_HINT_RANGE, "100,5000,10"), "set_rest_density", "get_rest_density");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "viscosity_coeff", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_viscosity_coeff", "get_viscosity_coeff");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "particle_radius", PROPERTY_HINT_RANGE, "0.005,0.1,0.005"), "set_particle_radius", "get_particle_radius");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "bounds_min"), "set_bounds_min", "get_bounds_min");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "bounds_max"), "set_bounds_max", "get_bounds_max");

    ADD_GROUP("Simulation", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulate"), "set_simulate", "get_simulate");
}

void CascadeFluid::set_num_particles(int p) { num_particles = CLAMP(p, 64, 65536); }
int CascadeFluid::get_num_particles() const { return num_particles; }
void CascadeFluid::set_simulate(bool p) {
    simulate = p;
    if (simulate && !gpu_initialized) _init_gpu();
}
bool CascadeFluid::get_simulate() const { return simulate; }
void CascadeFluid::set_smoothing_radius(float p) { smoothing_radius = MAX(p, 0.01f); }
float CascadeFluid::get_smoothing_radius() const { return smoothing_radius; }
void CascadeFluid::set_rest_density(float p) { rest_density = MAX(p, 1.0f); }
float CascadeFluid::get_rest_density() const { return rest_density; }
void CascadeFluid::set_viscosity_coeff(float p) { viscosity_coeff = MAX(p, 0.0f); }
float CascadeFluid::get_viscosity_coeff() const { return viscosity_coeff; }
void CascadeFluid::set_particle_radius(float p) { particle_radius = MAX(p, 0.001f); }
float CascadeFluid::get_particle_radius() const { return particle_radius; }
void CascadeFluid::set_bounds_min(Vector3 p) { bounds_min = p; }
Vector3 CascadeFluid::get_bounds_min() const { return bounds_min; }
void CascadeFluid::set_bounds_max(Vector3 p) { bounds_max = p; }
Vector3 CascadeFluid::get_bounds_max() const { return bounds_max; }

void CascadeFluid::_ready() {
    UtilityFunctions::print("[Cascade] CascadeFluid ready. Particles: ", num_particles);
}

void CascadeFluid::_process(double delta) {
    if (!simulate || !gpu_initialized) return;

    float dt = 1.0f / 120.0f; // SPH needs smaller timesteps for stability
    sim_time += dt;

    _simulate_step(dt);
    _update_multimesh();
}

// -------------------------------------------------------------------
// GPU helpers
// -------------------------------------------------------------------

RID CascadeFluid::_compile_shader(const char *source) {
    Ref<RDShaderSource> src;
    src.instantiate();
    src->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source);

    Ref<RDShaderSPIRV> spirv = local_rd->shader_compile_spirv_from_source(src);
    String err = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
    if (!err.is_empty()) {
        UtilityFunctions::printerr("[Cascade] Fluid shader error: ", err);
        return RID();
    }
    return local_rd->shader_create_from_spirv(spirv);
}

RID CascadeFluid::_make_uniform_set(RID shader,
    const std::vector<std::pair<int, RID>> &bindings) {
    Array uniforms;
    for (auto &[binding, rid] : bindings) {
        Ref<RDUniform> u;
        u.instantiate();
        u->set_uniform_type(binding == 5 ?
            RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER :
            RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u->set_binding(binding);
        u->add_id(rid);
        uniforms.push_back(u);
    }
    return local_rd->uniform_set_create(uniforms, shader, 0);
}

void CascadeFluid::_update_params(float dt) {
    PackedByteArray p;
    p.resize(80);
    uint8_t *d = p.ptrw();

    auto wu = [&](int off, uint32_t v) { memcpy(d + off, &v, 4); };
    auto wf = [&](int off, float v) { memcpy(d + off, &v, 4); };

    wu(0, (uint32_t)num_particles);
    wf(4, smoothing_radius);
    wf(8, rest_density);
    wf(12, gas_constant);
    wf(16, viscosity_coeff);
    wf(20, surface_tension);
    wf(24, dt);
    wf(28, -9.81f);
    wf(32, smoothing_radius); // grid cell size = smoothing radius
    wu(36, grid_w);
    wu(40, grid_h);
    wu(44, grid_d);
    wf(48, (float)bounds_min.x);
    wf(52, (float)bounds_min.y);
    wf(56, (float)bounds_min.z);
    wf(60, (float)bounds_max.x);
    wf(64, (float)bounds_max.y);
    wf(68, (float)bounds_max.z);
    wf(72, sim_time);
    wf(76, particle_mass);

    local_rd->buffer_update(params_buf, 0, 80, p);
}

// -------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------

void CascadeFluid::_init_gpu() {
    RenderingServer *rs = RenderingServer::get_singleton();
    local_rd = rs->create_local_rendering_device();
    if (!local_rd) {
        UtilityFunctions::printerr("[Cascade] Failed to create local RenderingDevice for fluid.");
        return;
    }

    // Compute grid dimensions
    Vector3 extent = bounds_max - bounds_min;
    grid_w = (uint32_t)std::ceil(extent.x / smoothing_radius) + 1;
    grid_h = (uint32_t)std::ceil(extent.y / smoothing_radius) + 1;
    grid_d = (uint32_t)std::ceil(extent.z / smoothing_radius) + 1;
    grid_total = grid_w * grid_h * grid_d;

    // Particle mass: compute so that rest density is achieved at the target particle spacing
    // For cubic kernel, the density at rest is approximately mass * W(0,h) * N_neighbors
    // Simpler: mass = rest_density * volume_per_particle
    float spacing = smoothing_radius * 0.5f;
    if (particle_mass <= 0.0f) {
        // Auto-compute: each particle represents a small volume of fluid
        float vol = (float)(bounds_max.x - bounds_min.x) * (float)(bounds_max.y - bounds_min.y) * (float)(bounds_max.z - bounds_min.z);
        particle_mass = rest_density * vol / (float)num_particles;
        // Clamp to reasonable range
        particle_mass = CLAMP(particle_mass, 0.001f, 1.0f);
    }

    UtilityFunctions::print("[Cascade] Fluid grid: ", (int)grid_w, "x", (int)grid_h, "x", (int)grid_d,
        " = ", (int)grid_total, " cells. Mass: ", particle_mass);

    // ---- Initial particle positions: block of fluid ----
    uint32_t part_buf_size = num_particles * 4 * sizeof(float);

    PackedByteArray pos_data;
    pos_data.resize(part_buf_size);
    float *pos_ptr = reinterpret_cast<float *>(pos_data.ptrw());

    // Arrange particles in a cubic block in the upper portion of the domain
    float spawn_spacing = smoothing_radius * 0.5f;
    int per_row = (int)std::cbrt((double)num_particles) + 1;

    Vector3 center = (bounds_min + bounds_max) * 0.5f;
    center.y = bounds_max.y * 0.7f; // spawn near top so fall is visible

    int placed = 0;
    for (int z = 0; z < per_row && placed < num_particles; z++) {
        for (int y = 0; y < per_row && placed < num_particles; y++) {
            for (int x = 0; x < per_row && placed < num_particles; x++) {
                int base = placed * 4;
                pos_ptr[base + 0] = center.x + (x - per_row / 2) * spawn_spacing;
                pos_ptr[base + 1] = center.y + (y - per_row / 2) * spawn_spacing;
                pos_ptr[base + 2] = center.z + (z - per_row / 2) * spawn_spacing;
                pos_ptr[base + 3] = rest_density; // initial density
                placed++;
            }
        }
    }

    PackedByteArray vel_data;
    vel_data.resize(part_buf_size);
    vel_data.fill(0);

    PackedByteArray force_data;
    force_data.resize(part_buf_size);
    force_data.fill(0);

    // grid_cells: uvec2 per cell (start, count)
    PackedByteArray grid_cell_data;
    grid_cell_data.resize(grid_total * 2 * sizeof(uint32_t));
    grid_cell_data.fill(0);

    // morton_pairs: uvec2 per particle (morton_code, particle_index)
    PackedByteArray morton_data;
    morton_data.resize(num_particles * 2 * sizeof(uint32_t));
    morton_data.fill(0);

    // cell_indices: uint per particle (particle index within cell range)
    PackedByteArray cell_indices_data;
    cell_indices_data.resize(num_particles * sizeof(uint32_t));
    cell_indices_data.fill(0);

    // cell_counters: uint per cell (atomic counter for scatter)
    PackedByteArray cell_counters_data;
    cell_counters_data.resize(grid_total * sizeof(uint32_t));
    cell_counters_data.fill(0);

    // ---- Create buffers ----
    positions_buf        = local_rd->storage_buffer_create(part_buf_size, pos_data);
    velocities_buf       = local_rd->storage_buffer_create(part_buf_size, vel_data);
    forces_buf           = local_rd->storage_buffer_create(part_buf_size, force_data);
    grid_cells_buf       = local_rd->storage_buffer_create(grid_total * 2 * sizeof(uint32_t), grid_cell_data);
    morton_pairs_buf     = local_rd->storage_buffer_create(num_particles * 2 * sizeof(uint32_t), morton_data);
    sorted_positions_buf = local_rd->storage_buffer_create(part_buf_size, vel_data);
    sorted_velocities_buf = local_rd->storage_buffer_create(part_buf_size, vel_data);
    cell_indices_buf     = local_rd->storage_buffer_create(num_particles * sizeof(uint32_t), cell_indices_data);
    cell_counters_buf    = local_rd->storage_buffer_create(grid_total * sizeof(uint32_t), cell_counters_data);

    PackedByteArray params_init;
    params_init.resize(80);
    params_init.fill(0);
    params_buf = local_rd->uniform_buffer_create(80, params_init);

    // ---- Compile shaders ----
    clear_grid_shader = _compile_shader(FLUID_CLEAR_GRID_SHADER);
    if (!clear_grid_shader.is_valid()) UtilityFunctions::printerr("[Cascade] clear_grid shader FAILED");
    sort_particles_shader = _compile_shader(FLUID_SORT_PARTICLES_SHADER);
    if (!sort_particles_shader.is_valid()) UtilityFunctions::printerr("[Cascade] sort_particles shader FAILED");
    bitonic_sort_shader = _compile_shader(FLUID_BITONIC_SORT_SHADER);
    if (!bitonic_sort_shader.is_valid()) UtilityFunctions::printerr("[Cascade] bitonic_sort shader FAILED");
    reorder_shader = _compile_shader(FLUID_REORDER_PARTICLES_SHADER);
    if (!reorder_shader.is_valid()) UtilityFunctions::printerr("[Cascade] reorder shader FAILED");
    copy_back_shader = _compile_shader(FLUID_COPY_BACK_SHADER);
    if (!copy_back_shader.is_valid()) UtilityFunctions::printerr("[Cascade] copy_back shader FAILED");
    build_grid_shader = _compile_shader(FLUID_BUILD_GRID_SHADER);
    if (!build_grid_shader.is_valid()) UtilityFunctions::printerr("[Cascade] build_grid shader FAILED");
    prefix_sum_shader = _compile_shader(FLUID_PREFIX_SUM_SHADER);
    if (!prefix_sum_shader.is_valid()) UtilityFunctions::printerr("[Cascade] prefix_sum shader FAILED");
    clear_counters_shader = _compile_shader(FLUID_CLEAR_COUNTERS_SHADER);
    if (!clear_counters_shader.is_valid()) UtilityFunctions::printerr("[Cascade] clear_counters shader FAILED");
    scatter_shader = _compile_shader(FLUID_SCATTER_SHADER);
    if (!scatter_shader.is_valid()) UtilityFunctions::printerr("[Cascade] scatter shader FAILED");
    density_shader    = _compile_shader(FLUID_DENSITY_SHADER);
    if (!density_shader.is_valid()) UtilityFunctions::printerr("[Cascade] density shader FAILED");
    forces_shader     = _compile_shader(FLUID_FORCES_SHADER);
    if (!forces_shader.is_valid()) UtilityFunctions::printerr("[Cascade] forces shader FAILED");
    integrate_shader  = _compile_shader(FLUID_INTEGRATE_SHADER);
    if (!integrate_shader.is_valid()) UtilityFunctions::printerr("[Cascade] integrate shader FAILED");

    bool all_ok = clear_grid_shader.is_valid() && sort_particles_shader.is_valid() &&
                  bitonic_sort_shader.is_valid() && reorder_shader.is_valid() &&
                  copy_back_shader.is_valid() && build_grid_shader.is_valid() &&
                  prefix_sum_shader.is_valid() && clear_counters_shader.is_valid() &&
                  scatter_shader.is_valid() && density_shader.is_valid() &&
                  forces_shader.is_valid() && integrate_shader.is_valid();
    if (!all_ok) {
        UtilityFunctions::printerr("[Cascade] One or more fluid shaders failed to compile.");
        return;
    }

    clear_grid_pipeline     = local_rd->compute_pipeline_create(clear_grid_shader);
    sort_particles_pipeline = local_rd->compute_pipeline_create(sort_particles_shader);
    bitonic_sort_pipeline   = local_rd->compute_pipeline_create(bitonic_sort_shader);
    reorder_pipeline        = local_rd->compute_pipeline_create(reorder_shader);
    copy_back_pipeline      = local_rd->compute_pipeline_create(copy_back_shader);
    build_grid_pipeline     = local_rd->compute_pipeline_create(build_grid_shader);
    prefix_sum_pipeline     = local_rd->compute_pipeline_create(prefix_sum_shader);
    clear_counters_pipeline = local_rd->compute_pipeline_create(clear_counters_shader);
    scatter_pipeline        = local_rd->compute_pipeline_create(scatter_shader);
    density_pipeline        = local_rd->compute_pipeline_create(density_shader);
    forces_pipeline         = local_rd->compute_pipeline_create(forces_shader);
    integrate_pipeline      = local_rd->compute_pipeline_create(integrate_shader);

    // ---- Uniform sets ----
    clear_grid_uset = _make_uniform_set(clear_grid_shader, {
        {3, grid_cells_buf}, {5, params_buf}
    });
    if (!clear_grid_uset.is_valid()) UtilityFunctions::printerr("[Cascade] clear_grid uset FAILED");

    sort_particles_uset = _make_uniform_set(sort_particles_shader, {
        {0, positions_buf}, {4, morton_pairs_buf}, {5, params_buf}
    });
    if (!sort_particles_uset.is_valid()) UtilityFunctions::printerr("[Cascade] sort_particles uset FAILED");

    bitonic_sort_uset = _make_uniform_set(bitonic_sort_shader, {
        {4, morton_pairs_buf}, {5, params_buf}
    });
    if (!bitonic_sort_uset.is_valid()) UtilityFunctions::printerr("[Cascade] bitonic_sort uset FAILED");

    reorder_uset = _make_uniform_set(reorder_shader, {
        {0, positions_buf}, {1, velocities_buf}, {4, morton_pairs_buf},
        {6, sorted_positions_buf}, {7, sorted_velocities_buf}, {5, params_buf}
    });
    if (!reorder_uset.is_valid()) UtilityFunctions::printerr("[Cascade] reorder uset FAILED");

    copy_back_uset = _make_uniform_set(copy_back_shader, {
        {0, positions_buf}, {1, velocities_buf},
        {6, sorted_positions_buf}, {7, sorted_velocities_buf}, {5, params_buf}
    });
    if (!copy_back_uset.is_valid()) UtilityFunctions::printerr("[Cascade] copy_back uset FAILED");

    build_grid_uset = _make_uniform_set(build_grid_shader, {
        {0, positions_buf}, {3, grid_cells_buf}, {5, params_buf}
    });
    if (!build_grid_uset.is_valid()) UtilityFunctions::printerr("[Cascade] build_grid uset FAILED");

    prefix_sum_uset = _make_uniform_set(prefix_sum_shader, {
        {3, grid_cells_buf}, {5, params_buf}
    });
    if (!prefix_sum_uset.is_valid()) UtilityFunctions::printerr("[Cascade] prefix_sum uset FAILED");

    clear_counters_uset = _make_uniform_set(clear_counters_shader, {
        {8, cell_counters_buf}, {5, params_buf}
    });
    if (!clear_counters_uset.is_valid()) UtilityFunctions::printerr("[Cascade] clear_counters uset FAILED");

    scatter_uset = _make_uniform_set(scatter_shader, {
        {0, positions_buf}, {3, grid_cells_buf}, {4, cell_indices_buf},
        {8, cell_counters_buf}, {5, params_buf}
    });
    if (!scatter_uset.is_valid()) UtilityFunctions::printerr("[Cascade] scatter uset FAILED");

    density_uset = _make_uniform_set(density_shader, {
        {0, positions_buf}, {1, velocities_buf}, {3, grid_cells_buf},
        {4, cell_indices_buf}, {5, params_buf}
    });
    if (!density_uset.is_valid()) UtilityFunctions::printerr("[Cascade] density uset FAILED");

    forces_uset = _make_uniform_set(forces_shader, {
        {0, positions_buf}, {1, velocities_buf}, {2, forces_buf},
        {3, grid_cells_buf}, {4, cell_indices_buf}, {5, params_buf}
    });
    if (!forces_uset.is_valid()) UtilityFunctions::printerr("[Cascade] forces uset FAILED");

    integrate_uset = _make_uniform_set(integrate_shader, {
        {0, positions_buf}, {1, velocities_buf}, {2, forces_buf}, {5, params_buf}
    });
    if (!integrate_uset.is_valid()) UtilityFunctions::printerr("[Cascade] integrate uset FAILED");

    // ---- Setup MultiMesh for rendering ----
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);

    Ref<SphereMesh> sphere;
    sphere.instantiate();
    sphere->set_radius(particle_radius);
    sphere->set_height(particle_radius * 2.0f);
    sphere->set_radial_segments(8);
    sphere->set_rings(4);
    mm->set_mesh(sphere);
    mm->set_instance_count(num_particles);

    set_multimesh(mm);

    gpu_initialized = true;
    UtilityFunctions::print("[Cascade] GPU SPH fluid initialized. Particles: ", num_particles);
}

// -------------------------------------------------------------------
// Simulation
// -------------------------------------------------------------------

void CascadeFluid::_simulate_step(float dt) {
    _update_params(dt);

    uint32_t pg = (num_particles + 63) / 64;
    uint32_t gg = (grid_total + 63) / 64;

    // Round num_particles up to next power of 2 for bitonic sort
    uint32_t n_padded = 1;
    while (n_padded < (uint32_t)num_particles) n_padded <<= 1;
    uint32_t sort_groups = (n_padded + 63) / 64;

    int64_t cl = local_rd->compute_list_begin();

    // 1. Compute Morton codes
    local_rd->compute_list_bind_compute_pipeline(cl, sort_particles_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, sort_particles_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 2. Bitonic sort on Morton codes
    local_rd->compute_list_bind_compute_pipeline(cl, bitonic_sort_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, bitonic_sort_uset, 0);
    for (uint32_t k = 2; k <= n_padded; k <<= 1) {
        for (uint32_t j = k >> 1; j > 0; j >>= 1) {
            PackedByteArray push_data;
            push_data.resize(16);
            push_data.fill(0);
            memcpy(push_data.ptrw(), &k, 4);
            memcpy(push_data.ptrw() + 4, &j, 4);
            local_rd->compute_list_set_push_constant(cl, push_data, 16);
            local_rd->compute_list_dispatch(cl, sort_groups, 1, 1);
            local_rd->compute_list_add_barrier(cl);
        }
    }

    // 3. Reorder particles into sorted order
    local_rd->compute_list_bind_compute_pipeline(cl, reorder_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, reorder_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 3b. Copy sorted data back to primary buffers
    local_rd->compute_list_bind_compute_pipeline(cl, copy_back_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, copy_back_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 4. Clear grid
    local_rd->compute_list_bind_compute_pipeline(cl, clear_grid_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, clear_grid_uset, 0);
    local_rd->compute_list_dispatch(cl, gg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 5. Build grid (count particles per cell)
    local_rd->compute_list_bind_compute_pipeline(cl, build_grid_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, build_grid_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 5b. Prefix sum to compute start offsets
    local_rd->compute_list_bind_compute_pipeline(cl, prefix_sum_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, prefix_sum_uset, 0);
    local_rd->compute_list_dispatch(cl, 1, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 5c. Clear counters before scatter
    local_rd->compute_list_bind_compute_pipeline(cl, clear_counters_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, clear_counters_uset, 0);
    local_rd->compute_list_dispatch(cl, gg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 5d. Scatter particle indices into cell ranges
    local_rd->compute_list_bind_compute_pipeline(cl, scatter_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, scatter_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 6. Compute density + pressure
    local_rd->compute_list_bind_compute_pipeline(cl, density_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, density_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 7. Compute forces
    local_rd->compute_list_bind_compute_pipeline(cl, forces_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, forces_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // 8. Integrate
    local_rd->compute_list_bind_compute_pipeline(cl, integrate_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, integrate_uset, 0);
    local_rd->compute_list_dispatch(cl, pg, 1, 1);

    local_rd->compute_list_end();
    local_rd->submit();
    local_rd->sync();
}

// -------------------------------------------------------------------
// MultiMesh update
// -------------------------------------------------------------------

void CascadeFluid::_update_multimesh() {
    PackedByteArray pos_bytes = local_rd->buffer_get_data(positions_buf);
    const float *pos = reinterpret_cast<const float *>(pos_bytes.ptr());

    Ref<MultiMesh> mm = get_multimesh();
    if (!mm.is_valid()) return;

    // Also read velocities for speed-based coloring
    PackedByteArray vel_bytes = local_rd->buffer_get_data(velocities_buf);
    const float *vel = reinterpret_cast<const float *>(vel_bytes.ptr());

    // Particle scale: slightly larger than spacing for overlap (liquid look)
    float scale = particle_radius * 2.5f;

    for (int i = 0; i < num_particles; i++) {
        int base = i * 4;

        // Scale transform for overlapping spheres
        Transform3D t;
        t.basis = Basis().scaled(Vector3(scale, scale, scale));
        t.origin = Vector3(pos[base], pos[base + 1], pos[base + 2]);
        mm->set_instance_transform(i, t);

        // Color: blue base, whiter at high velocity (foam-like), darker at rest
        float vx = vel[base], vy = vel[base + 1], vz = vel[base + 2];
        float speed = std::sqrt(vx * vx + vy * vy + vz * vz);
        float speed_ratio = CLAMP(speed * 0.5f, 0.0f, 1.0f);

        // Deep blue at rest → lighter blue/white at high speed
        float r = 0.05f + speed_ratio * 0.6f;
        float g = 0.15f + speed_ratio * 0.5f;
        float b = 0.6f + speed_ratio * 0.35f;
        mm->set_instance_color(i, Color(r, g, b, 1.0f));
    }
}

// -------------------------------------------------------------------
// Cleanup
// -------------------------------------------------------------------

void CascadeFluid::_cleanup_gpu() {
    if (!gpu_initialized || !local_rd) return;

    auto fr = [&](RID &rid) {
        if (rid.is_valid()) { local_rd->free_rid(rid); rid = RID(); }
    };

    fr(clear_grid_uset); fr(sort_particles_uset); fr(bitonic_sort_uset);
    fr(reorder_uset); fr(copy_back_uset); fr(build_grid_uset);
    fr(prefix_sum_uset); fr(clear_counters_uset); fr(scatter_uset);
    fr(density_uset); fr(forces_uset); fr(integrate_uset);

    fr(clear_grid_pipeline); fr(sort_particles_pipeline); fr(bitonic_sort_pipeline);
    fr(reorder_pipeline); fr(copy_back_pipeline); fr(build_grid_pipeline);
    fr(prefix_sum_pipeline); fr(clear_counters_pipeline); fr(scatter_pipeline);
    fr(density_pipeline); fr(forces_pipeline); fr(integrate_pipeline);

    fr(clear_grid_shader); fr(sort_particles_shader); fr(bitonic_sort_shader);
    fr(reorder_shader); fr(copy_back_shader); fr(build_grid_shader);
    fr(prefix_sum_shader); fr(clear_counters_shader); fr(scatter_shader);
    fr(density_shader); fr(forces_shader); fr(integrate_shader);

    fr(positions_buf); fr(velocities_buf); fr(forces_buf);
    fr(grid_cells_buf); fr(morton_pairs_buf); fr(params_buf);
    fr(sorted_positions_buf); fr(sorted_velocities_buf);
    fr(cell_indices_buf); fr(cell_counters_buf);

    memdelete(local_rd);
    local_rd = nullptr;
    gpu_initialized = false;
}
