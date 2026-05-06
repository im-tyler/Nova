#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>

#include <vector>

namespace godot {

class CascadeCloth : public MeshInstance3D {
    GDCLASS(CascadeCloth, MeshInstance3D)

public:
    CascadeCloth();
    ~CascadeCloth();

    void _ready() override;
    void _process(double delta) override;

    // Properties
    void set_width(int p_width);
    int get_width() const;
    void set_height(int p_height);
    int get_height() const;
    void set_spacing(float p_spacing);
    float get_spacing() const;
    void set_iterations(int p_iterations);
    int get_iterations() const;
    void set_simulate(bool p_simulate);
    bool get_simulate() const;
    void set_pin_mode(int p_mode);
    int get_pin_mode() const;
    void set_wind(Vector3 p_wind);
    Vector3 get_wind() const;
    void set_wind_turbulence(float p_turb);
    float get_wind_turbulence() const;
    void set_stretch_compliance(float p_val);
    float get_stretch_compliance() const;
    void set_bend_compliance(float p_val);
    float get_bend_compliance() const;

    // Mesh input — simulate an arbitrary mesh as cloth
    void set_source_mesh(Ref<Mesh> p_mesh);
    Ref<Mesh> get_source_mesh() const;

    // Pin specific vertices by index
    void pin_vertex(int index);
    void unpin_vertex(int index);
    void pin_vertices_above(float y_threshold);

    // Skeletal mesh binding
    void set_skeleton_path(NodePath p_path);
    NodePath get_skeleton_path() const;
    void bind_vertex_to_bone(int vertex_index, int bone_index);

    // Collider management
    void add_sphere_collider(Vector3 center, float radius);
    void add_plane_collider(Vector3 normal, float distance);
    void clear_colliders();

protected:
    static void _bind_methods();

private:
    // Source mesh mode
    Ref<Mesh> source_mesh;
    bool use_source_mesh = false;
    std::vector<uint32_t> pinned_vertices;

    // Mesh topology (for source mesh mode)
    PackedVector3Array mesh_vertices;
    PackedVector3Array mesh_normals;
    PackedInt32Array mesh_indices;
    struct Edge { uint32_t a, b; float rest_length; };
    std::vector<Edge> mesh_edges;

    // Simulation parameters
    int cloth_width = 32;
    int cloth_height = 32;
    float cloth_spacing = 0.08f;
    int solver_iterations = 10;
    bool simulate = false;
    int pin_mode = 0;
    Vector3 wind = Vector3(0, 0, 0);
    float wind_turbulence = 0.0f;
    float stretch_compliance = 0.0f;     // 0 = perfectly stiff
    float bend_compliance = 0.001f;      // softer bending
    float friction = 0.3f;

    // Collider data
    struct SphereCollider { float x, y, z, radius; };
    struct PlaneCollider { float nx, ny, nz, d; };
    std::vector<SphereCollider> sphere_colliders;
    std::vector<PlaneCollider> plane_colliders;
    bool colliders_dirty = true;

    // Constraint coloring
    struct ConstraintGroup {
        uint32_t offset;
        uint32_t count;
    };
    std::vector<ConstraintGroup> constraint_groups;
    uint32_t total_constraints = 0;

    // GPU state
    RenderingDevice *local_rd = nullptr;

    // Shaders and pipelines
    RID predict_shader, predict_pipeline;
    RID reset_lambda_shader, reset_lambda_pipeline;
    RID solve_shader, solve_pipeline;
    RID collide_shader, collide_pipeline;
    RID update_shader, update_pipeline;
    RID normals_shader, normals_pipeline;

    // Buffers
    RID positions_buf;    // binding 0
    RID predicted_buf;    // binding 1
    RID velocities_buf;   // binding 2
    RID constraints_buf;  // binding 3
    RID params_buf;       // binding 4
    RID normals_buf;      // binding 5
    RID colliders_buf;    // binding 6
    RID lambdas_buf;      // binding 7

    // Uniform sets
    RID predict_uset;
    RID reset_lambda_uset;
    RID solve_uset;
    RID collide_uset;
    RID update_uset;
    RID normals_uset;

    // Self-collision
    RID sc_clear_shader, sc_clear_pipeline;
    RID sc_build_shader, sc_build_pipeline;
    RID sc_resolve_shader, sc_resolve_pipeline;
    RID sc_grid_heads_buf;
    RID sc_grid_next_buf;
    RID sc_clear_uset, sc_build_uset, sc_resolve_uset;
    uint32_t sc_grid_size = 32;
    uint32_t sc_grid_total = 0;
    float cloth_thickness = 0.02f;

    // Skeletal mesh binding
    NodePath skeleton_path;
    Skeleton3D *skeleton = nullptr;
    struct BoneBinding { uint32_t vertex_index; int bone_index; };
    std::vector<BoneBinding> bone_bindings;

    bool gpu_initialized = false;
    int vertex_count = 0;
    float sim_time = 0.0f;
    bool default_material_applied = false;

    // Optimized mesh update: build mesh once, update vertex data each frame
    bool mesh_built = false;
    RID mesh_rid;  // RenderingServer mesh RID for direct vertex updates
    int render_vertex_count = 0; // total verts in render mesh (includes back faces)
    uint32_t vertex_stride = 0;  // bytes per vertex in the render mesh

    // Internal methods
    void _init_gpu();
    RID _compile_shader(const char *source);
    RID _make_uniform_set(RID shader, const std::vector<std::pair<int, RID>> &bindings);
    void _upload_colliders();
    void _simulate_step(float dt);
    void _update_params(float dt, uint32_t con_offset, uint32_t con_count);
    void _update_mesh_from_gpu();
    void _cleanup_gpu();
};

}
