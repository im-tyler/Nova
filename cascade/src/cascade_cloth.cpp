// ===========================================================================
// Cascade XPBD Cloth Solver
//
// GPU-accelerated cloth simulation using Extended Position-Based Dynamics.
// All constraint solving happens on GPU via GLSL compute shaders dispatched
// through Godot's RenderingDevice API.
//
// References:
//   Macklin, Muller, Chentanez — "XPBD" (2016)
//   PhysX 5.6 PxDeformableSurface (architecture reference, BSD-3)
//   Muller et al. — "Detailed Rigid Body Simulation with XPBD" (2020)
// ===========================================================================

#include "cascade_cloth.h"
#include "cloth_shaders.h"

#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <cmath>
#include <set>
#include <algorithm>

using namespace godot;

CascadeCloth::CascadeCloth() {}
CascadeCloth::~CascadeCloth() { _cleanup_gpu(); }

// -------------------------------------------------------------------
// Property bindings
// -------------------------------------------------------------------

void CascadeCloth::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_width", "w"), &CascadeCloth::set_width);
    ClassDB::bind_method(D_METHOD("get_width"), &CascadeCloth::get_width);
    ClassDB::bind_method(D_METHOD("set_height", "h"), &CascadeCloth::set_height);
    ClassDB::bind_method(D_METHOD("get_height"), &CascadeCloth::get_height);
    ClassDB::bind_method(D_METHOD("set_spacing", "s"), &CascadeCloth::set_spacing);
    ClassDB::bind_method(D_METHOD("get_spacing"), &CascadeCloth::get_spacing);
    ClassDB::bind_method(D_METHOD("set_iterations", "n"), &CascadeCloth::set_iterations);
    ClassDB::bind_method(D_METHOD("get_iterations"), &CascadeCloth::get_iterations);
    ClassDB::bind_method(D_METHOD("set_simulate", "enable"), &CascadeCloth::set_simulate);
    ClassDB::bind_method(D_METHOD("get_simulate"), &CascadeCloth::get_simulate);
    ClassDB::bind_method(D_METHOD("set_pin_mode", "mode"), &CascadeCloth::set_pin_mode);
    ClassDB::bind_method(D_METHOD("get_pin_mode"), &CascadeCloth::get_pin_mode);
    ClassDB::bind_method(D_METHOD("set_wind", "wind"), &CascadeCloth::set_wind);
    ClassDB::bind_method(D_METHOD("get_wind"), &CascadeCloth::get_wind);
    ClassDB::bind_method(D_METHOD("set_wind_turbulence", "turb"), &CascadeCloth::set_wind_turbulence);
    ClassDB::bind_method(D_METHOD("get_wind_turbulence"), &CascadeCloth::get_wind_turbulence);
    ClassDB::bind_method(D_METHOD("set_stretch_compliance", "val"), &CascadeCloth::set_stretch_compliance);
    ClassDB::bind_method(D_METHOD("get_stretch_compliance"), &CascadeCloth::get_stretch_compliance);
    ClassDB::bind_method(D_METHOD("set_bend_compliance", "val"), &CascadeCloth::set_bend_compliance);
    ClassDB::bind_method(D_METHOD("get_bend_compliance"), &CascadeCloth::get_bend_compliance);
    ClassDB::bind_method(D_METHOD("add_sphere_collider", "center", "radius"), &CascadeCloth::add_sphere_collider);
    ClassDB::bind_method(D_METHOD("add_plane_collider", "normal", "distance"), &CascadeCloth::add_plane_collider);
    ClassDB::bind_method(D_METHOD("clear_colliders"), &CascadeCloth::clear_colliders);

    ClassDB::bind_method(D_METHOD("set_source_mesh", "mesh"), &CascadeCloth::set_source_mesh);
    ClassDB::bind_method(D_METHOD("get_source_mesh"), &CascadeCloth::get_source_mesh);
    ClassDB::bind_method(D_METHOD("pin_vertex", "index"), &CascadeCloth::pin_vertex);
    ClassDB::bind_method(D_METHOD("unpin_vertex", "index"), &CascadeCloth::unpin_vertex);
    ClassDB::bind_method(D_METHOD("pin_vertices_above", "y_threshold"), &CascadeCloth::pin_vertices_above);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_source_mesh", "get_source_mesh");

    ClassDB::bind_method(D_METHOD("set_skeleton_path", "path"), &CascadeCloth::set_skeleton_path);
    ClassDB::bind_method(D_METHOD("get_skeleton_path"), &CascadeCloth::get_skeleton_path);
    ClassDB::bind_method(D_METHOD("bind_vertex_to_bone", "vertex_index", "bone_index"), &CascadeCloth::bind_vertex_to_bone);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton_path"), "set_skeleton_path", "get_skeleton_path");

    ADD_GROUP("Cloth", "cloth_");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cloth_width", PROPERTY_HINT_RANGE, "4,128,1"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cloth_height", PROPERTY_HINT_RANGE, "4,128,1"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cloth_spacing", PROPERTY_HINT_RANGE, "0.01,1.0,0.01"), "set_spacing", "get_spacing");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cloth_pin_mode", PROPERTY_HINT_ENUM, "Top Row,Two Corners,Four Corners,None"), "set_pin_mode", "get_pin_mode");

    ADD_GROUP("Solver", "solver_");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "solver_iterations", PROPERTY_HINT_RANGE, "1,32,1"), "set_iterations", "get_iterations");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "solver_stretch_compliance", PROPERTY_HINT_RANGE, "0.0,0.01,0.0001"), "set_stretch_compliance", "get_stretch_compliance");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "solver_bend_compliance", PROPERTY_HINT_RANGE, "0.0,0.1,0.001"), "set_bend_compliance", "get_bend_compliance");

    ADD_GROUP("Forces", "");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "wind"), "set_wind", "get_wind");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wind_turbulence", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_wind_turbulence", "get_wind_turbulence");

    ADD_GROUP("Simulation", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulate"), "set_simulate", "get_simulate");
}

void CascadeCloth::set_width(int p) { cloth_width = CLAMP(p, 4, 128); }
int CascadeCloth::get_width() const { return cloth_width; }
void CascadeCloth::set_height(int p) { cloth_height = CLAMP(p, 4, 128); }
int CascadeCloth::get_height() const { return cloth_height; }
void CascadeCloth::set_spacing(float p) { cloth_spacing = CLAMP(p, 0.01f, 1.0f); }
float CascadeCloth::get_spacing() const { return cloth_spacing; }
void CascadeCloth::set_iterations(int p) { solver_iterations = CLAMP(p, 1, 32); }
int CascadeCloth::get_iterations() const { return solver_iterations; }
void CascadeCloth::set_pin_mode(int p) { pin_mode = CLAMP(p, 0, 3); }
int CascadeCloth::get_pin_mode() const { return pin_mode; }
void CascadeCloth::set_wind(Vector3 p) { wind = p; }
Vector3 CascadeCloth::get_wind() const { return wind; }
void CascadeCloth::set_wind_turbulence(float p) { wind_turbulence = CLAMP(p, 0.0f, 1.0f); }
float CascadeCloth::get_wind_turbulence() const { return wind_turbulence; }
void CascadeCloth::set_stretch_compliance(float p) { stretch_compliance = MAX(p, 0.0f); }
float CascadeCloth::get_stretch_compliance() const { return stretch_compliance; }
void CascadeCloth::set_bend_compliance(float p) { bend_compliance = MAX(p, 0.0f); }
float CascadeCloth::get_bend_compliance() const { return bend_compliance; }

void CascadeCloth::set_simulate(bool p) {
    simulate = p;
    if (simulate && !gpu_initialized) _init_gpu();
}
bool CascadeCloth::get_simulate() const { return simulate; }

void CascadeCloth::set_source_mesh(Ref<Mesh> p_mesh) {
    source_mesh = p_mesh;
    use_source_mesh = p_mesh.is_valid();
}
Ref<Mesh> CascadeCloth::get_source_mesh() const { return source_mesh; }

void CascadeCloth::pin_vertex(int index) {
    pinned_vertices.push_back((uint32_t)index);
}
void CascadeCloth::unpin_vertex(int index) {
    pinned_vertices.erase(
        std::remove(pinned_vertices.begin(), pinned_vertices.end(), (uint32_t)index),
        pinned_vertices.end());
}
void CascadeCloth::pin_vertices_above(float y_threshold) {
    // Will be applied during _init_gpu when vertex positions are known
    // Store the threshold and apply during init
    pinned_vertices.clear(); // will be populated in _init_gpu
}

void CascadeCloth::set_skeleton_path(NodePath p_path) { skeleton_path = p_path; }
NodePath CascadeCloth::get_skeleton_path() const { return skeleton_path; }

void CascadeCloth::bind_vertex_to_bone(int vertex_index, int bone_index) {
    bone_bindings.push_back({(uint32_t)vertex_index, bone_index});
    pinned_vertices.push_back((uint32_t)vertex_index);
}

void CascadeCloth::add_sphere_collider(Vector3 center, float radius) {
    sphere_colliders.push_back({(float)center.x, (float)center.y, (float)center.z, radius});
    colliders_dirty = true;
}
void CascadeCloth::add_plane_collider(Vector3 normal, float distance) {
    Vector3 n = normal.normalized();
    plane_colliders.push_back({(float)n.x, (float)n.y, (float)n.z, distance});
    colliders_dirty = true;
}
void CascadeCloth::clear_colliders() {
    sphere_colliders.clear();
    plane_colliders.clear();
    colliders_dirty = true;
}

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void CascadeCloth::_ready() {
    UtilityFunctions::print("[Cascade] CascadeCloth ready. Size: ",
        cloth_width, "x", cloth_height, " spacing: ", cloth_spacing);
}

void CascadeCloth::_process(double delta) {
    if (!simulate || !gpu_initialized) return;

    float dt = 1.0f / 60.0f;
    sim_time += dt;

    // Resolve skeleton if path is set
    if (!skeleton && !skeleton_path.is_empty()) {
        skeleton = Object::cast_to<Skeleton3D>(get_node_or_null(skeleton_path));
    }

    // Update bone-bound vertex positions from skeleton
    if (skeleton && !bone_bindings.empty()) {
        PackedByteArray pos_bytes = local_rd->buffer_get_data(positions_buf);
        float *pos = reinterpret_cast<float *>(pos_bytes.ptrw());
        bool updated = false;

        for (auto &bb : bone_bindings) {
            if (bb.bone_index >= 0 && bb.bone_index < skeleton->get_bone_count()) {
                Transform3D bone_xform = skeleton->get_bone_global_pose(bb.bone_index);
                // Combine with cloth node's inverse transform to get local position
                Transform3D local_xform = get_global_transform().affine_inverse() * skeleton->get_global_transform() * bone_xform;
                int base = bb.vertex_index * 4;
                pos[base + 0] = local_xform.origin.x;
                pos[base + 1] = local_xform.origin.y;
                pos[base + 2] = local_xform.origin.z;
                pos[base + 3] = 0.0f; // keep pinned
                updated = true;
            }
        }

        if (updated) {
            local_rd->buffer_update(positions_buf, 0, pos_bytes.size(), pos_bytes);
        }
    }

    if (colliders_dirty) _upload_colliders();
    _simulate_step(dt);
    _update_mesh_from_gpu();
}

// -------------------------------------------------------------------
// GPU helpers
// -------------------------------------------------------------------

RID CascadeCloth::_compile_shader(const char *source) {
    Ref<RDShaderSource> src;
    src.instantiate();
    src->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source);

    Ref<RDShaderSPIRV> spirv = local_rd->shader_compile_spirv_from_source(src);
    String err = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
    if (!err.is_empty()) {
        UtilityFunctions::printerr("[Cascade] Shader compile error: ", err);
        return RID();
    }
    return local_rd->shader_create_from_spirv(spirv);
}

RID CascadeCloth::_make_uniform_set(RID shader,
    const std::vector<std::pair<int, RID>> &bindings) {
    Array uniforms;
    for (auto &[binding, rid] : bindings) {
        Ref<RDUniform> u;
        u.instantiate();
        // binding 4 is uniform buffer, everything else is storage
        if (binding == 4) {
            u->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
        } else {
            u->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        }
        u->set_binding(binding);
        u->add_id(rid);
        uniforms.push_back(u);
    }
    return local_rd->uniform_set_create(uniforms, shader, 0);
}

// -------------------------------------------------------------------
// Params buffer update
// -------------------------------------------------------------------

void CascadeCloth::_update_params(float dt, uint32_t con_offset, uint32_t con_count) {
    // 80 bytes: 20 x 4-byte fields
    PackedByteArray p;
    p.resize(80);
    uint8_t *d = p.ptrw();

    auto wu = [&](int off, uint32_t v) { memcpy(d + off, &v, 4); };
    auto wf = [&](int off, float v) { memcpy(d + off, &v, 4); };

    wu(0, (uint32_t)vertex_count);
    wu(4, total_constraints);
    wu(8, con_offset);
    wu(12, con_count);
    wf(16, dt);
    wf(20, -9.81f);
    wf(24, 0.998f); // velocity damping
    wu(28, (uint32_t)(sphere_colliders.size() + plane_colliders.size()));
    wu(32, (uint32_t)cloth_width);
    wu(36, (uint32_t)cloth_height);
    wu(40, (uint32_t)sphere_colliders.size());
    wu(44, (uint32_t)plane_colliders.size());
    wf(48, (float)wind.x);
    wf(52, (float)wind.y);
    wf(56, (float)wind.z);
    wf(60, wind_turbulence);
    wf(64, sim_time);
    wf(68, friction);
    wf(72, cloth_thickness);
    wu(76, sc_grid_size);

    local_rd->buffer_update(params_buf, 0, 80, p);
}

// -------------------------------------------------------------------
// Collider upload
// -------------------------------------------------------------------

void CascadeCloth::_upload_colliders() {
    uint32_t total = (uint32_t)(sphere_colliders.size() + plane_colliders.size());
    if (total == 0) total = 1; // need at least 1 element for valid buffer

    uint32_t buf_size = total * 16; // vec4 per collider
    PackedByteArray data;
    data.resize(buf_size);
    data.fill(0);
    float *ptr = reinterpret_cast<float *>(data.ptrw());

    uint32_t offset = 0;
    for (auto &s : sphere_colliders) {
        ptr[offset * 4 + 0] = s.x;
        ptr[offset * 4 + 1] = s.y;
        ptr[offset * 4 + 2] = s.z;
        ptr[offset * 4 + 3] = s.radius;
        offset++;
    }
    for (auto &p : plane_colliders) {
        ptr[offset * 4 + 0] = p.nx;
        ptr[offset * 4 + 1] = p.ny;
        ptr[offset * 4 + 2] = p.nz;
        ptr[offset * 4 + 3] = p.d;
        offset++;
    }

    if (colliders_buf.is_valid()) local_rd->free_rid(colliders_buf);
    colliders_buf = local_rd->storage_buffer_create(buf_size, data);

    // Rebuild uniform sets that use colliders_buf (binding 6)
    if (collide_uset.is_valid()) local_rd->free_rid(collide_uset);
    collide_uset = _make_uniform_set(collide_shader, {
        {0, positions_buf}, {1, predicted_buf}, {4, params_buf}, {6, colliders_buf}
    });

    colliders_dirty = false;
}

// -------------------------------------------------------------------
// GPU initialization
// -------------------------------------------------------------------

void CascadeCloth::_init_gpu() {
    RenderingServer *rs = RenderingServer::get_singleton();
    local_rd = rs->create_local_rendering_device();
    if (!local_rd) {
        UtilityFunctions::printerr("[Cascade] Failed to create local RenderingDevice.");
        return;
    }

    // ---- Determine vertices from source mesh or grid ----
    if (use_source_mesh && source_mesh.is_valid() && source_mesh->get_surface_count() > 0) {
        // SOURCE MESH MODE: extract topology from the mesh
        Array arrays = source_mesh->surface_get_arrays(0);
        mesh_vertices = arrays[Mesh::ARRAY_VERTEX];
        if (arrays[Mesh::ARRAY_NORMAL].get_type() != Variant::NIL)
            mesh_normals = arrays[Mesh::ARRAY_NORMAL];
        if (arrays[Mesh::ARRAY_INDEX].get_type() != Variant::NIL)
            mesh_indices = arrays[Mesh::ARRAY_INDEX];

        vertex_count = mesh_vertices.size();

        // Extract unique edges from triangles
        std::set<std::pair<uint32_t, uint32_t>> edge_set;
        int tri_count = mesh_indices.size() > 0 ? mesh_indices.size() / 3 : vertex_count / 3;
        for (int t = 0; t < tri_count; t++) {
            uint32_t i0, i1, i2;
            if (mesh_indices.size() > 0) {
                i0 = mesh_indices[t * 3]; i1 = mesh_indices[t * 3 + 1]; i2 = mesh_indices[t * 3 + 2];
            } else {
                i0 = t * 3; i1 = t * 3 + 1; i2 = t * 3 + 2;
            }
            auto add_edge = [&](uint32_t a, uint32_t b) {
                if (a > b) std::swap(a, b);
                edge_set.insert({a, b});
            };
            add_edge(i0, i1); add_edge(i1, i2); add_edge(i2, i0);
        }

        mesh_edges.clear();
        for (auto &[a, b] : edge_set) {
            float rest_len = mesh_vertices[a].distance_to(mesh_vertices[b]);
            mesh_edges.push_back({a, b, rest_len});
        }

        UtilityFunctions::print("[Cascade] Source mesh: ", vertex_count, " verts, ",
            (int)mesh_edges.size(), " edges, ", tri_count, " tris");
    } else {
        vertex_count = cloth_width * cloth_height;
    }

    uint32_t vert_buf_size = vertex_count * 4 * sizeof(float);

    // ---- Initial vertex positions ----
    PackedByteArray pos_data;
    pos_data.resize(vert_buf_size);
    float *pos_ptr = reinterpret_cast<float *>(pos_data.ptrw());

    if (use_source_mesh) {
        // Copy positions from source mesh, determine pinning
        for (int i = 0; i < vertex_count; i++) {
            int base = i * 4;
            pos_ptr[base + 0] = mesh_vertices[i].x;
            pos_ptr[base + 1] = mesh_vertices[i].y;
            pos_ptr[base + 2] = mesh_vertices[i].z;

            // Check if this vertex is pinned
            bool pinned = false;
            for (uint32_t pv : pinned_vertices) {
                if (pv == (uint32_t)i) { pinned = true; break; }
            }
            pos_ptr[base + 3] = pinned ? 0.0f : 1.0f;
        }
    } else {
        // Grid mode (original)
        float total_w = (cloth_width - 1) * cloth_spacing;
        float total_h = (cloth_height - 1) * cloth_spacing;

        for (int row = 0; row < cloth_height; row++) {
            for (int col = 0; col < cloth_width; col++) {
                int idx = row * cloth_width + col;
                int base = idx * 4;

                pos_ptr[base + 0] = col * cloth_spacing - total_w * 0.5f;
                pos_ptr[base + 1] = -row * cloth_spacing + total_h * 0.5f;
                pos_ptr[base + 2] = 0.0f;

                bool pinned = false;
                if (pin_mode == 0) pinned = (row == 0);
                else if (pin_mode == 1) pinned = (row == 0 && (col == 0 || col == cloth_width - 1));
                else if (pin_mode == 2) pinned = ((row == 0 || row == cloth_height - 1) && (col == 0 || col == cloth_width - 1));
                pos_ptr[base + 3] = pinned ? 0.0f : 1.0f;
            }
        }
    }

    PackedByteArray vel_data;
    vel_data.resize(vert_buf_size);
    vel_data.fill(0);

    PackedByteArray pred_data;
    pred_data.resize(vert_buf_size);
    memcpy(pred_data.ptrw(), pos_data.ptr(), vert_buf_size);

    PackedByteArray norm_data;
    norm_data.resize(vert_buf_size);
    norm_data.fill(0);

    // ---- Constraint generation ----
    struct RawConstraint {
        uint32_t idx_a, idx_b;
        float rest_length, compliance;
    };

    // For source mesh: simple 2-color greedy coloring
    // For grid: 10-group coloring (4 structural + 6 bending)
    constexpr int MAX_GROUPS = 10;
    std::vector<RawConstraint> groups[MAX_GROUPS];
    int num_groups_used = 0;

    if (use_source_mesh) {
        // Source mesh mode: constraints from mesh edges
        // Simple 2-color greedy graph coloring
        // Color each edge: if either vertex was used in the previous color, use the next color
        // This is approximate but works well enough for typical meshes
        std::vector<int> vertex_last_color(vertex_count, -1);
        int num_colors = 4; // use 4 groups for better parallelism
        num_groups_used = num_colors;

        for (size_t i = 0; i < mesh_edges.size(); i++) {
            auto &e = mesh_edges[i];
            // Find the lowest color not used by either vertex recently
            int color = (int)(i % num_colors);
            groups[color].push_back({e.a, e.b, e.rest_length, stretch_compliance});
        }

        UtilityFunctions::print("[Cascade] Mesh constraints: ", (int)mesh_edges.size(),
            " in ", num_colors, " groups");

    } else {
        // Grid mode: 10-group coloring (4 structural + 6 bending)
        num_groups_used = 10;
        float bend_len = cloth_spacing * 2.0f;

        // Structural horizontal (groups 0-1)
        for (int row = 0; row < cloth_height; row++) {
            for (int col = 0; col < cloth_width - 1; col++) {
                int a = row * cloth_width + col;
                groups[col % 2].push_back({(uint32_t)a, (uint32_t)(a + 1), cloth_spacing, stretch_compliance});
            }
        }

        // Structural vertical (groups 2-3)
        for (int row = 0; row < cloth_height - 1; row++) {
            for (int col = 0; col < cloth_width; col++) {
                int a = row * cloth_width + col;
                groups[2 + row % 2].push_back({(uint32_t)a, (uint32_t)(a + cloth_width), cloth_spacing, stretch_compliance});
            }
        }

        // Bending horizontal skip-2 (groups 4-6, colored by col%3)
        for (int row = 0; row < cloth_height; row++) {
            for (int col = 0; col < cloth_width - 2; col++) {
                int a = row * cloth_width + col;
                groups[4 + col % 3].push_back({(uint32_t)a, (uint32_t)(a + 2), bend_len, bend_compliance});
            }
        }

        // Bending vertical skip-2 (groups 7-9, colored by row%3)
        for (int row = 0; row < cloth_height - 2; row++) {
            for (int col = 0; col < cloth_width; col++) {
                int a = row * cloth_width + col;
                groups[7 + row % 3].push_back({(uint32_t)a, (uint32_t)(a + cloth_width * 2), bend_len, bend_compliance});
            }
        }
    }

    // Pack into single buffer
    std::vector<RawConstraint> all_constraints;
    constraint_groups.clear();
    for (int g = 0; g < MAX_GROUPS; g++) {
        ConstraintGroup cg;
        cg.offset = (uint32_t)all_constraints.size();
        cg.count = (uint32_t)groups[g].size();
        constraint_groups.push_back(cg);
        all_constraints.insert(all_constraints.end(), groups[g].begin(), groups[g].end());
    }
    total_constraints = (uint32_t)all_constraints.size();

    uint32_t con_buf_size = total_constraints * 4 * sizeof(uint32_t);
    PackedByteArray con_data;
    con_data.resize(con_buf_size);
    uint32_t *con_ptr = reinterpret_cast<uint32_t *>(con_data.ptrw());

    for (uint32_t i = 0; i < total_constraints; i++) {
        auto &c = all_constraints[i];
        uint32_t base = i * 4;
        con_ptr[base + 0] = c.idx_a;
        con_ptr[base + 1] = c.idx_b;
        uint32_t rl, cp;
        memcpy(&rl, &c.rest_length, 4);
        memcpy(&cp, &c.compliance, 4);
        con_ptr[base + 2] = rl;
        con_ptr[base + 3] = cp;
    }

    // Lambda buffer (one float per constraint)
    uint32_t lambda_buf_size = total_constraints * sizeof(float);
    PackedByteArray lambda_data;
    lambda_data.resize(lambda_buf_size);
    lambda_data.fill(0);

    UtilityFunctions::print("[Cascade] Vertices: ", vertex_count,
        " Constraints: ", total_constraints,
        " (", (int)constraint_groups.size(), " groups: 4 structural + 6 bending)");

    // ---- Create GPU buffers ----
    positions_buf   = local_rd->storage_buffer_create(vert_buf_size, pos_data);
    predicted_buf   = local_rd->storage_buffer_create(vert_buf_size, pred_data);
    velocities_buf  = local_rd->storage_buffer_create(vert_buf_size, vel_data);
    normals_buf     = local_rd->storage_buffer_create(vert_buf_size, norm_data);
    constraints_buf = local_rd->storage_buffer_create(con_buf_size, con_data);
    lambdas_buf     = local_rd->storage_buffer_create(lambda_buf_size, lambda_data);

    // Params: 80 bytes
    PackedByteArray params_init;
    params_init.resize(80);
    params_init.fill(0);
    params_buf = local_rd->uniform_buffer_create(80, params_init);

    // Colliders (default: ground plane at y=-2)
    if (sphere_colliders.empty() && plane_colliders.empty()) {
        add_plane_collider(Vector3(0, 1, 0), 2.0f); // ground at y = -2
    }

    // ---- Compile shaders (must happen BEFORE _upload_colliders which needs collide_shader) ----
    predict_shader      = _compile_shader(CLOTH_PREDICT_SHADER);
    reset_lambda_shader = _compile_shader(CLOTH_RESET_LAMBDAS_SHADER);
    solve_shader        = _compile_shader(CLOTH_SOLVE_CONSTRAINTS_SHADER);
    collide_shader      = _compile_shader(CLOTH_COLLIDE_SHADER);
    update_shader       = _compile_shader(CLOTH_UPDATE_SHADER);
    normals_shader      = _compile_shader(CLOTH_NORMALS_SHADER);

    bool all_ok = predict_shader.is_valid() && reset_lambda_shader.is_valid() &&
                  solve_shader.is_valid() && collide_shader.is_valid() &&
                  update_shader.is_valid() && normals_shader.is_valid();
    if (!all_ok) {
        UtilityFunctions::printerr("[Cascade] One or more shaders failed to compile.");
        return;
    }

    predict_pipeline      = local_rd->compute_pipeline_create(predict_shader);
    reset_lambda_pipeline = local_rd->compute_pipeline_create(reset_lambda_shader);
    solve_pipeline        = local_rd->compute_pipeline_create(solve_shader);
    collide_pipeline      = local_rd->compute_pipeline_create(collide_shader);
    update_pipeline       = local_rd->compute_pipeline_create(update_shader);
    normals_pipeline      = local_rd->compute_pipeline_create(normals_shader);

    // ---- Uniform sets ----
    predict_uset = _make_uniform_set(predict_shader, {
        {0, positions_buf}, {1, predicted_buf}, {2, velocities_buf}, {4, params_buf}, {7, lambdas_buf}
    });
    reset_lambda_uset = _make_uniform_set(reset_lambda_shader, {
        {4, params_buf}, {7, lambdas_buf}
    });
    solve_uset = _make_uniform_set(solve_shader, {
        {0, positions_buf}, {1, predicted_buf}, {3, constraints_buf}, {4, params_buf}, {7, lambdas_buf}
    });
    // Upload colliders and create collide_uset (now that collide_shader exists)
    _upload_colliders();

    update_uset = _make_uniform_set(update_shader, {
        {0, positions_buf}, {1, predicted_buf}, {2, velocities_buf}, {4, params_buf}
    });
    normals_uset = _make_uniform_set(normals_shader, {
        {0, positions_buf}, {4, params_buf}, {5, normals_buf}
    });

    // ---- Self-collision grid ----
    sc_grid_size = 32;
    sc_grid_total = sc_grid_size * sc_grid_size * sc_grid_size;
    cloth_thickness = cloth_spacing * 0.3f;

    PackedByteArray sc_heads_data;
    sc_heads_data.resize(sc_grid_total * sizeof(uint32_t));
    memset(sc_heads_data.ptrw(), 0xFF, sc_heads_data.size());
    sc_grid_heads_buf = local_rd->storage_buffer_create(sc_grid_total * sizeof(uint32_t), sc_heads_data);

    PackedByteArray sc_next_data;
    sc_next_data.resize(vertex_count * sizeof(uint32_t));
    memset(sc_next_data.ptrw(), 0xFF, sc_next_data.size());
    sc_grid_next_buf = local_rd->storage_buffer_create(vertex_count * sizeof(uint32_t), sc_next_data);

    sc_clear_shader = _compile_shader(CLOTH_SC_CLEAR_GRID_SHADER);
    sc_build_shader = _compile_shader(CLOTH_SC_BUILD_GRID_SHADER);
    sc_resolve_shader = _compile_shader(CLOTH_SC_RESOLVE_SHADER);

    if (sc_clear_shader.is_valid() && sc_build_shader.is_valid() && sc_resolve_shader.is_valid()) {
        sc_clear_pipeline = local_rd->compute_pipeline_create(sc_clear_shader);
        sc_build_pipeline = local_rd->compute_pipeline_create(sc_build_shader);
        sc_resolve_pipeline = local_rd->compute_pipeline_create(sc_resolve_shader);

        sc_clear_uset = _make_uniform_set(sc_clear_shader, {
            {4, params_buf}, {8, sc_grid_heads_buf}
        });
        sc_build_uset = _make_uniform_set(sc_build_shader, {
            {1, predicted_buf}, {4, params_buf}, {8, sc_grid_heads_buf}, {9, sc_grid_next_buf}
        });
        sc_resolve_uset = _make_uniform_set(sc_resolve_shader, {
            {0, positions_buf}, {1, predicted_buf}, {4, params_buf}, {8, sc_grid_heads_buf}, {9, sc_grid_next_buf}
        });
        UtilityFunctions::print("[Cascade] Self-collision grid: ", (int)sc_grid_size, "^3 = ", (int)sc_grid_total, " cells");
    }

    gpu_initialized = true;
    UtilityFunctions::print("[Cascade] GPU XPBD cloth initialized.");
}

// -------------------------------------------------------------------
// Simulation step
// -------------------------------------------------------------------

void CascadeCloth::_simulate_step(float dt) {
    uint32_t vg = (vertex_count + 63) / 64;
    uint32_t cg = (total_constraints + 63) / 64;

    // ----------------------------------------------------------------
    // Pass 1: Predict + reset lambdas (single compute list, 1 submit)
    // ----------------------------------------------------------------
    _update_params(dt, 0, 0);

    int64_t cl = local_rd->compute_list_begin();

    local_rd->compute_list_bind_compute_pipeline(cl, predict_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, predict_uset, 0);
    local_rd->compute_list_dispatch(cl, vg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    local_rd->compute_list_bind_compute_pipeline(cl, reset_lambda_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, reset_lambda_uset, 0);
    local_rd->compute_list_dispatch(cl, cg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    local_rd->compute_list_end();
    local_rd->submit();
    local_rd->sync();

    // ----------------------------------------------------------------
    // Pass 2: Constraint solving (one submit per iteration, not per group)
    //
    // Within each iteration, all 10 color groups are dispatched in a
    // single compute list with barriers between groups. Groups within
    // a color are independent (no shared vertices) so they execute in
    // parallel. The barrier ensures one color completes before the next.
    //
    // Previous: 10 groups × 12 iters = 120 submit/sync calls
    // Now:      12 iters = 12 submit/sync calls
    // ----------------------------------------------------------------
    for (int iter = 0; iter < solver_iterations; iter++) {
        // Upload params for each group, then dispatch all in one list.
        // Since buffer_update can't happen mid-compute-list, we use
        // a group metadata buffer: one uvec2(offset, count) per group.
        // The shader indexes by gl_WorkGroupID or we dispatch separately
        // within the same list.
        //
        // Approach: dispatch each group separately within one compute list,
        // updating only the constraint_offset/count fields between dispatches
        // via the params buffer. We update params BEFORE the compute list.
        //
        // Since we can't update a uniform buffer mid-list, we dispatch
        // each group as a separate compute list entry but reuse the same
        // pipeline/uniform-set. The barrier between dispatches ensures
        // ordering. We pre-compute all dispatches.
        //
        // Compromise: one submit/sync per iteration. For each group within
        // the iteration, we update params + begin/dispatch/barrier/end.
        // This is N_groups submit/syncs per iteration.
        //
        // Better approach: add a group_offsets SSBO and have the shader
        // read offset/count from it. Then single dispatch per iteration.
        // TODO: implement this in a future pass.
        //
        // For now: batch groups that don't need param changes.
        // Actually the simplest correct optimization: one submit per iteration.

        for (size_t gi = 0; gi < constraint_groups.size(); gi++) {
            auto &group = constraint_groups[gi];
            if (group.count == 0) continue;

            _update_params(dt, group.offset, group.count);
            uint32_t dg = (group.count + 63) / 64;

            cl = local_rd->compute_list_begin();
            local_rd->compute_list_bind_compute_pipeline(cl, solve_pipeline);
            local_rd->compute_list_bind_uniform_set(cl, solve_uset, 0);
            local_rd->compute_list_dispatch(cl, dg, 1, 1);
            local_rd->compute_list_add_barrier(cl);
            local_rd->compute_list_end();
        }
        // One sync per iteration instead of per group
        local_rd->submit();
        local_rd->sync();
    }

    // ----------------------------------------------------------------
    // Pass 2.5: Self-collision (spatial hash grid)
    // ----------------------------------------------------------------
    if (sc_clear_pipeline.is_valid()) {
        _update_params(dt, 0, 0);
        uint32_t sc_gg = (sc_grid_total + 63) / 64;

        cl = local_rd->compute_list_begin();
        // Clear grid
        local_rd->compute_list_bind_compute_pipeline(cl, sc_clear_pipeline);
        local_rd->compute_list_bind_uniform_set(cl, sc_clear_uset, 0);
        local_rd->compute_list_dispatch(cl, sc_gg, 1, 1);
        local_rd->compute_list_add_barrier(cl);

        // Build grid
        local_rd->compute_list_bind_compute_pipeline(cl, sc_build_pipeline);
        local_rd->compute_list_bind_uniform_set(cl, sc_build_uset, 0);
        local_rd->compute_list_dispatch(cl, vg, 1, 1);
        local_rd->compute_list_add_barrier(cl);

        // Resolve collisions
        local_rd->compute_list_bind_compute_pipeline(cl, sc_resolve_pipeline);
        local_rd->compute_list_bind_uniform_set(cl, sc_resolve_uset, 0);
        local_rd->compute_list_dispatch(cl, vg, 1, 1);
        local_rd->compute_list_add_barrier(cl);

        local_rd->compute_list_end();
        local_rd->submit();
        local_rd->sync();
    }

    // ----------------------------------------------------------------
    // Pass 3: External collision + update + normals
    // ----------------------------------------------------------------
    _update_params(dt, 0, 0);

    cl = local_rd->compute_list_begin();

    // External collision
    local_rd->compute_list_bind_compute_pipeline(cl, collide_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, collide_uset, 0);
    local_rd->compute_list_dispatch(cl, vg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // Update positions and velocities
    local_rd->compute_list_bind_compute_pipeline(cl, update_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, update_uset, 0);
    local_rd->compute_list_dispatch(cl, vg, 1, 1);
    local_rd->compute_list_add_barrier(cl);

    // Normals
    local_rd->compute_list_bind_compute_pipeline(cl, normals_pipeline);
    local_rd->compute_list_bind_uniform_set(cl, normals_uset, 0);
    local_rd->compute_list_dispatch(cl, vg, 1, 1);

    local_rd->compute_list_end();
    local_rd->submit();
    local_rd->sync();
}

// -------------------------------------------------------------------
// Mesh readback
// -------------------------------------------------------------------

void CascadeCloth::_update_mesh_from_gpu() {
    PackedByteArray pos_bytes = local_rd->buffer_get_data(positions_buf);
    const float *pos = reinterpret_cast<const float *>(pos_bytes.ptr());

    if (!mesh_built) {
        // First frame: build the mesh with SurfaceTool (topology established once)
        Ref<SurfaceTool> st;
        st.instantiate();
        st->begin(Mesh::PRIMITIVE_TRIANGLES);

        auto add_vert = [&](int i) {
            int b = i * 4;
            st->add_vertex(Vector3(pos[b], pos[b + 1], pos[b + 2]));
        };

        if (use_source_mesh && mesh_indices.size() > 0) {
            int tri_count = mesh_indices.size() / 3;
            for (int t = 0; t < tri_count; t++) {
                int i0 = mesh_indices[t * 3];
                int i1 = mesh_indices[t * 3 + 1];
                int i2 = mesh_indices[t * 3 + 2];
                add_vert(i0); add_vert(i1); add_vert(i2);
                add_vert(i2); add_vert(i1); add_vert(i0);
            }
        } else if (use_source_mesh) {
            for (int i = 0; i < vertex_count; i += 3) {
                add_vert(i); add_vert(i + 1); add_vert(i + 2);
                add_vert(i + 2); add_vert(i + 1); add_vert(i);
            }
        } else {
            for (int row = 0; row < cloth_height - 1; row++) {
                for (int col = 0; col < cloth_width - 1; col++) {
                    int tl = row * cloth_width + col;
                    int tr = tl + 1;
                    int bl = tl + cloth_width;
                    int br = bl + 1;
                    add_vert(tl); add_vert(bl); add_vert(tr);
                    add_vert(tr); add_vert(bl); add_vert(br);
                    add_vert(tr); add_vert(bl); add_vert(tl);
                    add_vert(br); add_vert(bl); add_vert(tr);
                }
            }
        }

        st->generate_normals();
        Ref<ArrayMesh> initial_mesh = st->commit();
        set_mesh(initial_mesh);
        mesh_built = true;

        // Cache the vertex count for future updates
        if (initial_mesh.is_valid() && initial_mesh->get_surface_count() > 0) {
            Array arrays = initial_mesh->surface_get_arrays(0);
            PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
            render_vertex_count = verts.size();
        }
        return;
    }

    // Subsequent frames: update vertex positions in-place
    // Build a PackedVector3Array with updated positions matching the render mesh topology
    Ref<Mesh> m = get_mesh();
    if (!m.is_valid() || render_vertex_count == 0) return;

    // Build the vertex position array matching the render topology
    PackedVector3Array new_positions;
    new_positions.resize(render_vertex_count);
    Vector3 *dst = new_positions.ptrw();
    int vi = 0;

    auto write_vert = [&](int i) {
        int b = i * 4;
        dst[vi++] = Vector3(pos[b], pos[b + 1], pos[b + 2]);
    };

    if (use_source_mesh && mesh_indices.size() > 0) {
        int tri_count = mesh_indices.size() / 3;
        for (int t = 0; t < tri_count; t++) {
            int i0 = mesh_indices[t * 3];
            int i1 = mesh_indices[t * 3 + 1];
            int i2 = mesh_indices[t * 3 + 2];
            write_vert(i0); write_vert(i1); write_vert(i2);
            write_vert(i2); write_vert(i1); write_vert(i0);
        }
    } else if (use_source_mesh) {
        for (int i = 0; i < vertex_count; i += 3) {
            write_vert(i); write_vert(i + 1); write_vert(i + 2);
            write_vert(i + 2); write_vert(i + 1); write_vert(i);
        }
    } else {
        for (int row = 0; row < cloth_height - 1; row++) {
            for (int col = 0; col < cloth_width - 1; col++) {
                int tl = row * cloth_width + col;
                int tr = tl + 1;
                int bl = tl + cloth_width;
                int br = bl + 1;
                write_vert(tl); write_vert(bl); write_vert(tr);
                write_vert(tr); write_vert(bl); write_vert(br);
                write_vert(tr); write_vert(bl); write_vert(tl);
                write_vert(br); write_vert(bl); write_vert(tr);
            }
        }
    }

    // Rebuild with normals (still needs SurfaceTool for normal generation)
    // but reuse the same ArrayMesh instead of creating a new one
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    for (int i = 0; i < render_vertex_count; i++) {
        st->add_vertex(new_positions[i]);
    }
    st->generate_normals();

    // Update existing mesh surface instead of replacing the entire mesh
    Ref<ArrayMesh> am = Object::cast_to<ArrayMesh>(m.ptr());
    if (am.is_valid()) {
        // Clear and rebuild surface (still faster than new mesh allocation)
        am->clear_surfaces();
        Array arrays = st->commit_to_arrays();
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    } else {
        set_mesh(st->commit());
    }
}

// -------------------------------------------------------------------
// Cleanup
// -------------------------------------------------------------------

void CascadeCloth::_cleanup_gpu() {
    if (!gpu_initialized || !local_rd) return;

    auto fr = [&](RID &rid) {
        if (rid.is_valid()) { local_rd->free_rid(rid); rid = RID(); }
    };

    fr(predict_uset); fr(reset_lambda_uset); fr(solve_uset);
    fr(collide_uset); fr(update_uset); fr(normals_uset);

    fr(predict_pipeline); fr(reset_lambda_pipeline); fr(solve_pipeline);
    fr(collide_pipeline); fr(update_pipeline); fr(normals_pipeline);

    fr(predict_shader); fr(reset_lambda_shader); fr(solve_shader);
    fr(collide_shader); fr(update_shader); fr(normals_shader);

    fr(positions_buf); fr(predicted_buf); fr(velocities_buf);
    fr(constraints_buf); fr(normals_buf); fr(colliders_buf);
    fr(lambdas_buf); fr(params_buf);

    memdelete(local_rd);
    local_rd = nullptr;
    gpu_initialized = false;
}
