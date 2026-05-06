#include "cascade_compute_test.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Simple compute shader that generates a waving cloth grid.
// Positions are computed entirely on GPU — no CPU vertex generation.
static const char *CLOTH_COMPUTE_SHADER = R"(
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict buffer PositionBuffer {
    float positions[];
};

layout(set = 0, binding = 1, std430) restrict buffer NormalBuffer {
    float normals[];
};

layout(set = 0, binding = 2, std140) uniform Params {
    int grid_size;
    float time;
    float spacing;
    float padding;
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint total = uint(grid_size * grid_size);
    if (idx >= total) return;

    uint row = idx / uint(grid_size);
    uint col = idx % uint(grid_size);

    float x = float(col) * spacing - float(grid_size) * spacing * 0.5;
    float z = float(row) * spacing - float(grid_size) * spacing * 0.5;

    // Simple wave displacement to prove GPU is computing positions
    float y = sin(x * 1.5 + time) * cos(z * 1.5 + time * 0.7) * 0.5;

    uint base = idx * 3u;
    positions[base + 0u] = x;
    positions[base + 1u] = y;
    positions[base + 2u] = z;

    // Approximate normal from wave partial derivatives
    float dydx = 1.5 * cos(x * 1.5 + time) * cos(z * 1.5 + time * 0.7) * 0.5;
    float dydz = -1.5 * sin(x * 1.5 + time) * sin(z * 1.5 + time * 0.7) * 0.5;
    vec3 n = normalize(vec3(-dydx, 1.0, -dydz));

    normals[base + 0u] = n.x;
    normals[base + 1u] = n.y;
    normals[base + 2u] = n.z;
}
)";

CascadeComputeTest::CascadeComputeTest() {}

CascadeComputeTest::~CascadeComputeTest() {
    _cleanup_compute();
}

void CascadeComputeTest::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &CascadeComputeTest::set_grid_size);
    ClassDB::bind_method(D_METHOD("get_grid_size"), &CascadeComputeTest::get_grid_size);
    ClassDB::bind_method(D_METHOD("set_simulate", "enable"), &CascadeComputeTest::set_simulate);
    ClassDB::bind_method(D_METHOD("get_simulate"), &CascadeComputeTest::get_simulate);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "grid_size", PROPERTY_HINT_RANGE, "4,128,1"), "set_grid_size", "get_grid_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulate"), "set_simulate", "get_simulate");
}

void CascadeComputeTest::set_grid_size(int p_size) {
    grid_size = CLAMP(p_size, 4, 128);
}

int CascadeComputeTest::get_grid_size() const {
    return grid_size;
}

void CascadeComputeTest::set_simulate(bool p_simulate) {
    simulate = p_simulate;
    if (simulate && !compute_initialized) {
        _init_compute();
    }
}

bool CascadeComputeTest::get_simulate() const {
    return simulate;
}

void CascadeComputeTest::_ready() {
    UtilityFunctions::print("[Cascade] CascadeComputeTest ready. Grid: ", grid_size, "x", grid_size);
}

void CascadeComputeTest::_process(double delta) {
    if (!simulate || !compute_initialized) return;

    elapsed_time += delta;
    _dispatch_compute(elapsed_time);
    _update_mesh();

    // Debug: print once per second
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 60 == 1) {
        Ref<Mesh> m = get_mesh();
        if (m.is_valid()) {
            UtilityFunctions::print("[Cascade] Frame ", frame_count,
                " mesh surfaces: ", m->get_surface_count(),
                " verts(approx): ", vertex_count);
        } else {
            UtilityFunctions::print("[Cascade] Frame ", frame_count, " - NO MESH");
        }
    }
}

void CascadeComputeTest::_init_compute() {
    RenderingServer *rs = RenderingServer::get_singleton();
    local_rd = rs->create_local_rendering_device();
    if (!local_rd) {
        UtilityFunctions::printerr("[Cascade] Failed to create local RenderingDevice.");
        return;
    }
    RenderingDevice *rd = local_rd;

    vertex_count = grid_size * grid_size;
    uint32_t float_count = vertex_count * 3;
    uint32_t buffer_size = float_count * sizeof(float);

    // Create storage buffers for positions and normals
    PackedByteArray pos_data;
    pos_data.resize(buffer_size);
    pos_data.fill(0);

    PackedByteArray norm_data;
    norm_data.resize(buffer_size);
    norm_data.fill(0);

    position_buffer_rid = rd->storage_buffer_create(buffer_size, pos_data);
    normal_buffer_rid = rd->storage_buffer_create(buffer_size, norm_data);

    // Create params uniform buffer (grid_size, time, spacing, padding)
    PackedByteArray params_data;
    params_data.resize(16); // 4 floats = 16 bytes (std140)
    params_buffer_rid = rd->uniform_buffer_create(16, params_data);

    // Compile compute shader
    Ref<RDShaderSource> shader_src;
    shader_src.instantiate();
    shader_src->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, CLOTH_COMPUTE_SHADER);

    Ref<RDShaderSPIRV> spirv = rd->shader_compile_spirv_from_source(shader_src);
    String compile_error = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
    if (!compile_error.is_empty()) {
        UtilityFunctions::printerr("[Cascade] Shader compile error: ", compile_error);
        return;
    }

    shader_rid = rd->shader_create_from_spirv(spirv);
    pipeline_rid = rd->compute_pipeline_create(shader_rid);

    // Create uniform set
    Array uniforms;

    Ref<RDUniform> u_pos;
    u_pos.instantiate();
    u_pos->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    u_pos->set_binding(0);
    u_pos->add_id(position_buffer_rid);
    uniforms.push_back(u_pos);

    Ref<RDUniform> u_norm;
    u_norm.instantiate();
    u_norm->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    u_norm->set_binding(1);
    u_norm->add_id(normal_buffer_rid);
    uniforms.push_back(u_norm);

    Ref<RDUniform> u_params;
    u_params.instantiate();
    u_params->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
    u_params->set_binding(2);
    u_params->add_id(params_buffer_rid);
    uniforms.push_back(u_params);

    uniform_set_rid = rd->uniform_set_create(uniforms, shader_rid, 0);

    compute_initialized = true;
    UtilityFunctions::print("[Cascade] Compute initialized. Vertices: ", vertex_count);
}

void CascadeComputeTest::_dispatch_compute(double time) {
    RenderingDevice *rd = local_rd;
    if (!rd) return;

    // Update params buffer
    PackedByteArray params_data;
    params_data.resize(16);
    int32_t gs = grid_size;
    float t = static_cast<float>(time);
    float spacing = 4.0f / static_cast<float>(grid_size);
    float pad = 0.0f;

    memcpy(params_data.ptrw(), &gs, 4);
    memcpy(params_data.ptrw() + 4, &t, 4);
    memcpy(params_data.ptrw() + 8, &spacing, 4);
    memcpy(params_data.ptrw() + 12, &pad, 4);

    rd->buffer_update(params_buffer_rid, 0, 16, params_data);

    // Dispatch compute
    uint32_t groups = (vertex_count + 63) / 64;
    int64_t compute_list = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline(compute_list, pipeline_rid);
    rd->compute_list_bind_uniform_set(compute_list, uniform_set_rid, 0);
    rd->compute_list_dispatch(compute_list, groups, 1, 1);
    rd->compute_list_end();

    // Submit and sync (blocking for now — optimization later)
    rd->submit();
    rd->sync();
}

void CascadeComputeTest::_update_mesh() {
    RenderingDevice *rd = local_rd;
    if (!rd) return;

    // Read back positions and normals from GPU
    PackedByteArray pos_bytes = rd->buffer_get_data(position_buffer_rid);
    PackedByteArray norm_bytes = rd->buffer_get_data(normal_buffer_rid);

    if (pos_bytes.size() == 0) {
        UtilityFunctions::printerr("[Cascade] buffer_get_data returned empty!");
        return;
    }

    const float *positions = reinterpret_cast<const float *>(pos_bytes.ptr());
    const float *normals_data = reinterpret_cast<const float *>(norm_bytes.ptr());

    // Debug: print first vertex position once
    static bool printed_first = false;
    if (!printed_first) {
        UtilityFunctions::print("[Cascade] First vertex: (",
            positions[0], ", ", positions[1], ", ", positions[2], ")");
        UtilityFunctions::print("[Cascade] Buffer size: ", pos_bytes.size(), " bytes, expected: ", vertex_count * 3 * 4);
        printed_first = true;
    }

    // Build mesh using SurfaceTool
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);

    // Generate indexed triangle grid
    for (int row = 0; row < grid_size - 1; row++) {
        for (int col = 0; col < grid_size - 1; col++) {
            int tl = row * grid_size + col;
            int tr = tl + 1;
            int bl = (row + 1) * grid_size + col;
            int br = bl + 1;

            auto add_vertex = [&](int idx) {
                int base = idx * 3;
                st->set_normal(Vector3(normals_data[base], normals_data[base + 1], normals_data[base + 2]));
                st->add_vertex(Vector3(positions[base], positions[base + 1], positions[base + 2]));
            };

            // Triangle 1
            add_vertex(tl);
            add_vertex(bl);
            add_vertex(tr);

            // Triangle 2
            add_vertex(tr);
            add_vertex(bl);
            add_vertex(br);
        }
    }

    Ref<ArrayMesh> mesh = st->commit();
    set_mesh(mesh);
}

void CascadeComputeTest::_cleanup_compute() {
    if (!compute_initialized) return;

    if (local_rd) {
        if (uniform_set_rid.is_valid()) local_rd->free_rid(uniform_set_rid);
        if (pipeline_rid.is_valid()) local_rd->free_rid(pipeline_rid);
        if (shader_rid.is_valid()) local_rd->free_rid(shader_rid);
        if (position_buffer_rid.is_valid()) local_rd->free_rid(position_buffer_rid);
        if (normal_buffer_rid.is_valid()) local_rd->free_rid(normal_buffer_rid);
        if (params_buffer_rid.is_valid()) local_rd->free_rid(params_buffer_rid);
        memdelete(local_rd);
        local_rd = nullptr;
    }

    compute_initialized = false;
}
