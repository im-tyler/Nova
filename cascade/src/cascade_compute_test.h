#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rendering_device.hpp>

namespace godot {

// Gate test node: dispatches a compute shader via RenderingDevice,
// writes vertex positions, and renders them as a mesh.
// This validates the critical path: GPU compute -> mesh rendering.
class CascadeComputeTest : public MeshInstance3D {
    GDCLASS(CascadeComputeTest, MeshInstance3D)

public:
    CascadeComputeTest();
    ~CascadeComputeTest();

    void _ready() override;
    void _process(double delta) override;

    void set_grid_size(int p_size);
    int get_grid_size() const;

    void set_simulate(bool p_simulate);
    bool get_simulate() const;

protected:
    static void _bind_methods();

private:
    void _init_compute();
    void _dispatch_compute(double time);
    void _update_mesh();
    void _cleanup_compute();

    int grid_size = 32;
    bool simulate = false;
    double elapsed_time = 0.0;

    // Local RenderingDevice for compute work
    RenderingDevice *local_rd = nullptr;

    // RenderingDevice resources (on local_rd)
    RID shader_rid;
    RID pipeline_rid;
    RID position_buffer_rid;
    RID normal_buffer_rid;
    RID params_buffer_rid;
    RID uniform_set_rid;

    bool compute_initialized = false;
    int vertex_count = 0;
};

}
