#pragma once

#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>

#include <vector>
#include <utility>

namespace godot {

// SPH fluid simulation using GPU compute shaders.
// Renders particles as instanced spheres via MultiMesh.
class CascadeFluid : public MultiMeshInstance3D {
    GDCLASS(CascadeFluid, MultiMeshInstance3D)

public:
    CascadeFluid();
    ~CascadeFluid();

    void _ready() override;
    void _process(double delta) override;

    void set_num_particles(int p);
    int get_num_particles() const;
    void set_simulate(bool p);
    bool get_simulate() const;
    void set_smoothing_radius(float p);
    float get_smoothing_radius() const;
    void set_rest_density(float p);
    float get_rest_density() const;
    void set_viscosity_coeff(float p);
    float get_viscosity_coeff() const;
    void set_particle_radius(float p);
    float get_particle_radius() const;
    void set_bounds_min(Vector3 p);
    Vector3 get_bounds_min() const;
    void set_bounds_max(Vector3 p);
    Vector3 get_bounds_max() const;

protected:
    static void _bind_methods();

private:
    // Parameters
    int num_particles = 2048;
    bool simulate = false;
    float smoothing_radius = 0.1f;
    float rest_density = 1000.0f;
    float gas_constant = 2.0f;         // Tait-like stiffness (low = compressible, high = stiff)
    float viscosity_coeff = 0.1f;       // XSPH viscosity
    float surface_tension = 0.0f;
    float particle_radius = 0.02f;
    float particle_mass = 0.0f;         // 0 = auto-compute from rest_density and spacing
    Vector3 bounds_min = Vector3(-1, -1, -1);
    Vector3 bounds_max = Vector3(1, 1, 1);

    // Grid
    uint32_t grid_w = 0, grid_h = 0, grid_d = 0;
    uint32_t grid_total = 0;

    // GPU
    RenderingDevice *local_rd = nullptr;

    RID clear_grid_shader, clear_grid_pipeline;
    RID sort_particles_shader, sort_particles_pipeline;
    RID bitonic_sort_shader, bitonic_sort_pipeline;
    RID reorder_shader, reorder_pipeline;
    RID copy_back_shader, copy_back_pipeline;
    RID build_grid_shader, build_grid_pipeline;
    RID prefix_sum_shader, prefix_sum_pipeline;
    RID clear_counters_shader, clear_counters_pipeline;
    RID scatter_shader, scatter_pipeline;
    RID density_shader, density_pipeline;
    RID forces_shader, forces_pipeline;
    RID integrate_shader, integrate_pipeline;

    RID positions_buf;
    RID velocities_buf;
    RID forces_buf;
    RID grid_cells_buf;
    RID morton_pairs_buf;
    RID params_buf;
    RID sorted_positions_buf;
    RID sorted_velocities_buf;
    RID cell_counters_buf;
    RID cell_indices_buf;

    RID clear_grid_uset;
    RID sort_particles_uset;
    RID bitonic_sort_uset;
    RID reorder_uset;
    RID copy_back_uset;
    RID build_grid_uset;
    RID prefix_sum_uset;
    RID clear_counters_uset;
    RID scatter_uset;
    RID density_uset;
    RID forces_uset;
    RID integrate_uset;

    bool gpu_initialized = false;
    float sim_time = 0.0f;

    void _init_gpu();
    RID _compile_shader(const char *source);
    RID _make_uniform_set(RID shader, const std::vector<std::pair<int, RID>> &bindings);
    void _update_params(float dt);
    void _simulate_step(float dt);
    void _update_multimesh();
    void _cleanup_gpu();
};

}
