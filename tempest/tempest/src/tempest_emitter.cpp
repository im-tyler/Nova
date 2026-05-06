#include "tempest_emitter.h"
#include "particle_shaders.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>

using namespace godot;

TempestEmitter::TempestEmitter() {}

TempestEmitter::~TempestEmitter() {
    _cleanup_compute();
}

void TempestEmitter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_num_particles", "count"), &TempestEmitter::set_num_particles);
    ClassDB::bind_method(D_METHOD("get_num_particles"), &TempestEmitter::get_num_particles);
    ClassDB::bind_method(D_METHOD("set_emission_rate", "rate"), &TempestEmitter::set_emission_rate);
    ClassDB::bind_method(D_METHOD("get_emission_rate"), &TempestEmitter::get_emission_rate);
    ClassDB::bind_method(D_METHOD("set_lifetime", "seconds"), &TempestEmitter::set_lifetime);
    ClassDB::bind_method(D_METHOD("get_lifetime"), &TempestEmitter::get_lifetime);
    ClassDB::bind_method(D_METHOD("set_gravity", "g"), &TempestEmitter::set_gravity);
    ClassDB::bind_method(D_METHOD("get_gravity"), &TempestEmitter::get_gravity);
    ClassDB::bind_method(D_METHOD("set_initial_velocity", "vel"), &TempestEmitter::set_initial_velocity);
    ClassDB::bind_method(D_METHOD("get_initial_velocity"), &TempestEmitter::get_initial_velocity);
    ClassDB::bind_method(D_METHOD("set_spread_angle", "radians"), &TempestEmitter::set_spread_angle);
    ClassDB::bind_method(D_METHOD("get_spread_angle"), &TempestEmitter::get_spread_angle);
    ClassDB::bind_method(D_METHOD("set_emission_shape", "shape"), &TempestEmitter::set_emission_shape);
    ClassDB::bind_method(D_METHOD("get_emission_shape"), &TempestEmitter::get_emission_shape);
    ClassDB::bind_method(D_METHOD("set_particle_size", "size"), &TempestEmitter::set_particle_size);
    ClassDB::bind_method(D_METHOD("get_particle_size"), &TempestEmitter::get_particle_size);
    ClassDB::bind_method(D_METHOD("set_color_start", "color"), &TempestEmitter::set_color_start);
    ClassDB::bind_method(D_METHOD("get_color_start"), &TempestEmitter::get_color_start);
    ClassDB::bind_method(D_METHOD("set_color_end", "color"), &TempestEmitter::set_color_end);
    ClassDB::bind_method(D_METHOD("get_color_end"), &TempestEmitter::get_color_end);
    ClassDB::bind_method(D_METHOD("set_emitting", "active"), &TempestEmitter::set_emitting);
    ClassDB::bind_method(D_METHOD("get_emitting"), &TempestEmitter::get_emitting);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_particles", PROPERTY_HINT_RANGE, "64,1000000,64"), "set_num_particles", "get_num_particles");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_rate", PROPERTY_HINT_RANGE, "1,100000,1"), "set_emission_rate", "get_emission_rate");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.1,60,0.1"), "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_gravity", "get_gravity");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "initial_velocity"), "set_initial_velocity", "get_initial_velocity");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread_angle", PROPERTY_HINT_RANGE, "0,3.14159,0.01"), "set_spread_angle", "get_spread_angle");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "emission_shape", PROPERTY_HINT_ENUM, "Point,Sphere,Box"), "set_emission_shape", "get_emission_shape");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "particle_size", PROPERTY_HINT_RANGE, "0.001,10,0.001"), "set_particle_size", "get_particle_size");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_start"), "set_color_start", "get_color_start");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_end"), "set_color_end", "get_color_end");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting"), "set_emitting", "get_emitting");

    ClassDB::bind_method(D_METHOD("set_drag", "drag"), &TempestEmitter::set_drag);
    ClassDB::bind_method(D_METHOD("get_drag"), &TempestEmitter::get_drag);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drag", PROPERTY_HINT_RANGE, "0.0,10.0,0.01"), "set_drag", "get_drag");

    ClassDB::bind_method(D_METHOD("add_attractor", "position", "strength", "radius", "falloff"), &TempestEmitter::add_attractor);
    ClassDB::bind_method(D_METHOD("add_repulsor", "position", "strength", "radius", "falloff"), &TempestEmitter::add_repulsor);
    ClassDB::bind_method(D_METHOD("add_vortex", "position", "axis", "strength", "radius", "falloff"), &TempestEmitter::add_vortex);
    ClassDB::bind_method(D_METHOD("add_turbulence", "position", "strength", "radius", "frequency"), &TempestEmitter::add_turbulence);
    ClassDB::bind_method(D_METHOD("add_wind", "direction", "strength", "radius"), &TempestEmitter::add_wind);
    ClassDB::bind_method(D_METHOD("clear_force_fields"), &TempestEmitter::clear_force_fields);

    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX);
}

// --- Property accessors ---

void TempestEmitter::set_num_particles(int p_num) {
    num_particles = CLAMP(p_num, 64, 1000000);
}
int TempestEmitter::get_num_particles() const { return num_particles; }

void TempestEmitter::set_emission_rate(float p_rate) { emission_rate = MAX(1.0f, p_rate); }
float TempestEmitter::get_emission_rate() const { return emission_rate; }

void TempestEmitter::set_lifetime(float p_lifetime) { lifetime = MAX(0.1f, p_lifetime); }
float TempestEmitter::get_lifetime() const { return lifetime; }

void TempestEmitter::set_gravity(float p_gravity) { gravity = p_gravity; }
float TempestEmitter::get_gravity() const { return gravity; }

void TempestEmitter::set_initial_velocity(Vector3 p_vel) { initial_velocity = p_vel; }
Vector3 TempestEmitter::get_initial_velocity() const { return initial_velocity; }

void TempestEmitter::set_spread_angle(float p_angle) { spread_angle = CLAMP(p_angle, 0.0f, 3.14159f); }
float TempestEmitter::get_spread_angle() const { return spread_angle; }

void TempestEmitter::set_emission_shape(int p_shape) { emission_shape = CLAMP(p_shape, 0, 2); }
int TempestEmitter::get_emission_shape() const { return emission_shape; }

void TempestEmitter::set_particle_size(float p_size) { particle_size = MAX(0.001f, p_size); }
float TempestEmitter::get_particle_size() const { return particle_size; }

void TempestEmitter::set_color_start(Color p_color) { color_start = p_color; }
Color TempestEmitter::get_color_start() const { return color_start; }

void TempestEmitter::set_color_end(Color p_color) { color_end = p_color; }
Color TempestEmitter::get_color_end() const { return color_end; }

void TempestEmitter::set_emitting(bool p_emitting) { emitting = p_emitting; }

void TempestEmitter::set_drag(float p_drag) { drag = CLAMP(p_drag, 0.0f, 10.0f); }
float TempestEmitter::get_drag() const { return drag; }

void TempestEmitter::add_attractor(Vector3 position, float strength, float radius, float falloff) {
    force_fields.push_back({0, strength, radius, falloff, (float)position.x, (float)position.y, (float)position.z, 0, 0, 0, 0, 0});
    force_fields_dirty = true;
}
void TempestEmitter::add_repulsor(Vector3 position, float strength, float radius, float falloff) {
    force_fields.push_back({1, strength, radius, falloff, (float)position.x, (float)position.y, (float)position.z, 0, 0, 0, 0, 0});
    force_fields_dirty = true;
}
void TempestEmitter::add_vortex(Vector3 position, Vector3 axis, float strength, float radius, float falloff) {
    force_fields.push_back({2, strength, radius, falloff, (float)position.x, (float)position.y, (float)position.z, 0, (float)axis.x, (float)axis.y, (float)axis.z, 0});
    force_fields_dirty = true;
}
void TempestEmitter::add_turbulence(Vector3 position, float strength, float radius, float frequency) {
    force_fields.push_back({3, strength, radius, 1.0f, (float)position.x, (float)position.y, (float)position.z, 0, 0, 0, 0, frequency});
    force_fields_dirty = true;
}
void TempestEmitter::add_wind(Vector3 direction, float strength, float radius) {
    force_fields.push_back({4, strength, radius, 1.0f, 0, 0, 0, 0, (float)direction.x, (float)direction.y, (float)direction.z, 0});
    force_fields_dirty = true;
}
void TempestEmitter::clear_force_fields() {
    force_fields.clear();
    force_fields_dirty = true;
}
bool TempestEmitter::get_emitting() const { return emitting; }

// --- Lifecycle ---

void TempestEmitter::_ready() {
    UtilityFunctions::print("[Tempest] TempestEmitter ready. Particles: ", num_particles);
    _init_multimesh();
    _init_compute();
}

void TempestEmitter::_process(double delta) {
    if (!compute_initialized) return;

    float dt = static_cast<float>(delta);
    frame_counter++;

    // Accumulate emissions
    if (emitting) {
        emit_accumulator += emission_rate * dt;
        int to_emit = static_cast<int>(emit_accumulator);
        if (to_emit > 0) {
            emit_accumulator -= static_cast<float>(to_emit);
            _dispatch_emit(to_emit);
        }
    }

    _dispatch_update(dt);
    _readback_and_update_multimesh();
}

// --- MultiMesh setup ---

void TempestEmitter::_init_multimesh() {
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);

    Ref<QuadMesh> quad;
    quad.instantiate();
    quad->set_size(Vector2(1.0f, 1.0f));

    // Material: billboard particles, vertex colors, alpha transparency
    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
    mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
    quad->set_material(mat);

    mm->set_mesh(quad);

    mm->set_instance_count(num_particles);
    mm->set_visible_instance_count(0);

    set_multimesh(mm);
}

// --- Compute init ---

static RID compile_shader(RenderingDevice *rd, const char *source, const char *name) {
    Ref<RDShaderSource> src;
    src.instantiate();
    src->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source);

    Ref<RDShaderSPIRV> spirv = rd->shader_compile_spirv_from_source(src);
    String err = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
    if (!err.is_empty()) {
        UtilityFunctions::printerr("[Tempest] Shader compile error (", name, "): ", err);
        return RID();
    }
    return rd->shader_create_from_spirv(spirv);
}

void TempestEmitter::_init_compute() {
    RenderingServer *rs = RenderingServer::get_singleton();
    local_rd = rs->create_local_rendering_device();
    if (!local_rd) {
        UtilityFunctions::printerr("[Tempest] Failed to create local RenderingDevice.");
        return;
    }
    RenderingDevice *rd = local_rd;

    uint32_t buf_size_vec4 = num_particles * 4 * sizeof(float); // vec4 per particle

    // Initialize all particles as dead (age = -1.0)
    PackedByteArray pos_data;
    pos_data.resize(buf_size_vec4);
    {
        float *ptr = reinterpret_cast<float *>(pos_data.ptrw());
        for (int i = 0; i < num_particles; i++) {
            ptr[i * 4 + 0] = 0.0f; // x
            ptr[i * 4 + 1] = 0.0f; // y
            ptr[i * 4 + 2] = 0.0f; // z
            ptr[i * 4 + 3] = -1.0f; // age = dead
        }
    }

    PackedByteArray vel_data;
    vel_data.resize(buf_size_vec4);
    vel_data.fill(0);

    PackedByteArray col_data;
    col_data.resize(buf_size_vec4);
    col_data.fill(0);

    position_buffer_rid = rd->storage_buffer_create(buf_size_vec4, pos_data);
    velocity_buffer_rid = rd->storage_buffer_create(buf_size_vec4, vel_data);
    color_buffer_rid = rd->storage_buffer_create(buf_size_vec4, col_data);

    // --- Update shader ---
    // Params: delta_time(4) + gravity(4) + num_particles(4) + num_force_fields(4) +
    //         color_start(16) + color_end(16) + time(4) + drag(4) + pad(8) = 64 bytes
    {
        PackedByteArray params;
        params.resize(64);
        params.fill(0);
        update_params_rid = rd->uniform_buffer_create(64, params);
    }

    // Force field buffer (at least 48 bytes even if empty — 1 dummy field)
    {
        PackedByteArray ff_data;
        ff_data.resize(48);
        ff_data.fill(0);
        force_field_buffer_rid = rd->storage_buffer_create(48, ff_data);
    }

    update_shader_rid = compile_shader(rd, PARTICLE_UPDATE_SHADER, "update");
    if (!update_shader_rid.is_valid()) return;
    update_pipeline_rid = rd->compute_pipeline_create(update_shader_rid);

    {
        Array uniforms;

        Ref<RDUniform> u_pos;
        u_pos.instantiate();
        u_pos->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_pos->set_binding(0);
        u_pos->add_id(position_buffer_rid);
        uniforms.push_back(u_pos);

        Ref<RDUniform> u_vel;
        u_vel.instantiate();
        u_vel->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_vel->set_binding(1);
        u_vel->add_id(velocity_buffer_rid);
        uniforms.push_back(u_vel);

        Ref<RDUniform> u_col;
        u_col.instantiate();
        u_col->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_col->set_binding(2);
        u_col->add_id(color_buffer_rid);
        uniforms.push_back(u_col);

        Ref<RDUniform> u_params;
        u_params.instantiate();
        u_params->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
        u_params->set_binding(3);
        u_params->add_id(update_params_rid);
        uniforms.push_back(u_params);

        Ref<RDUniform> u_ff;
        u_ff.instantiate();
        u_ff->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_ff->set_binding(4);
        u_ff->add_id(force_field_buffer_rid);
        uniforms.push_back(u_ff);

        update_uniform_set_rid = rd->uniform_set_create(uniforms, update_shader_rid, 0);
    }

    // --- Emit shader ---
    // EmitParams: emitter_position(16) + initial_velocity(16) + color_start(16) +
    //             lifetime(4) + particle_size(4) + emission_shape(4) + shape_radius(4) +
    //             frame_seed(4) + num_particles(4) + pad(4) + pad(4) = 80 bytes
    {
        PackedByteArray params;
        params.resize(80);
        params.fill(0);
        emit_params_rid = rd->uniform_buffer_create(80, params);
    }

    // Emit counter: single int (4 bytes), stored as storage buffer for atomic ops
    {
        PackedByteArray counter_data;
        counter_data.resize(4);
        counter_data.fill(0);
        emit_counter_rid = rd->storage_buffer_create(4, counter_data);
    }

    emit_shader_rid = compile_shader(rd, PARTICLE_EMIT_SHADER, "emit");
    if (!emit_shader_rid.is_valid()) return;
    emit_pipeline_rid = rd->compute_pipeline_create(emit_shader_rid);

    {
        Array uniforms;

        Ref<RDUniform> u_pos;
        u_pos.instantiate();
        u_pos->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_pos->set_binding(0);
        u_pos->add_id(position_buffer_rid);
        uniforms.push_back(u_pos);

        Ref<RDUniform> u_vel;
        u_vel.instantiate();
        u_vel->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_vel->set_binding(1);
        u_vel->add_id(velocity_buffer_rid);
        uniforms.push_back(u_vel);

        Ref<RDUniform> u_col;
        u_col.instantiate();
        u_col->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_col->set_binding(2);
        u_col->add_id(color_buffer_rid);
        uniforms.push_back(u_col);

        Ref<RDUniform> u_counter;
        u_counter.instantiate();
        u_counter->set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u_counter->set_binding(3);
        u_counter->add_id(emit_counter_rid);
        uniforms.push_back(u_counter);

        Ref<RDUniform> u_params;
        u_params.instantiate();
        u_params->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
        u_params->set_binding(4);
        u_params->add_id(emit_params_rid);
        uniforms.push_back(u_params);

        emit_uniform_set_rid = rd->uniform_set_create(uniforms, emit_shader_rid, 0);
    }

    compute_initialized = true;
    UtilityFunctions::print("[Tempest] Compute initialized. Particles: ", num_particles);
}

// --- Dispatch emit ---

void TempestEmitter::_dispatch_emit(int count) {
    RenderingDevice *rd = local_rd;
    if (!rd || count <= 0) return;

    // Set emit counter
    {
        PackedByteArray counter_data;
        counter_data.resize(4);
        int32_t c = count;
        memcpy(counter_data.ptrw(), &c, 4);
        rd->buffer_update(emit_counter_rid, 0, 4, counter_data);
    }

    // Update emit params
    {
        PackedByteArray params;
        params.resize(80);

        float *f = reinterpret_cast<float *>(params.ptrw());
        uint32_t *u = reinterpret_cast<uint32_t *>(params.ptrw());
        int32_t *i = reinterpret_cast<int32_t *>(params.ptrw());

        // Get emitter world position
        Vector3 pos = get_global_position();
        f[0] = pos.x;  f[1] = pos.y;  f[2] = pos.z;  f[3] = 0.0f;

        // Initial velocity + spread in w
        f[4] = initial_velocity.x;
        f[5] = initial_velocity.y;
        f[6] = initial_velocity.z;
        f[7] = spread_angle;

        // color_start
        f[8]  = color_start.r;
        f[9]  = color_start.g;
        f[10] = color_start.b;
        f[11] = color_start.a;

        // lifetime, particle_size, emission_shape, shape_radius
        f[12] = lifetime;
        f[13] = particle_size;
        i[14] = emission_shape;
        f[15] = 0.5f; // shape_radius default

        // frame_seed, num_particles, pad, pad
        u[16] = frame_counter;
        i[17] = num_particles;
        f[18] = 0.0f;
        f[19] = 0.0f;

        rd->buffer_update(emit_params_rid, 0, 80, params);
    }

    uint32_t groups = (num_particles + 255) / 256;
    int64_t cl = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline(cl, emit_pipeline_rid);
    rd->compute_list_bind_uniform_set(cl, emit_uniform_set_rid, 0);
    rd->compute_list_dispatch(cl, groups, 1, 1);
    rd->compute_list_end();
    rd->submit();
    rd->sync();
}

// --- Dispatch update ---

void TempestEmitter::_dispatch_update(float delta) {
    RenderingDevice *rd = local_rd;
    if (!rd) return;

    sim_time += delta;

    // Upload force field data if changed
    if (force_fields_dirty && force_field_buffer_rid.is_valid()) {
        uint32_t ff_count = (uint32_t)force_fields.size();
        uint32_t ff_size = MAX(ff_count, (uint32_t)1) * 48; // 3 vec4s per field
        PackedByteArray ff_data;
        ff_data.resize(ff_size);
        ff_data.fill(0);
        if (ff_count > 0) {
            memcpy(ff_data.ptrw(), force_fields.data(), ff_count * sizeof(ForceField));
        }
        // Recreate buffer if size changed
        rd->free_rid(force_field_buffer_rid);
        force_field_buffer_rid = rd->storage_buffer_create(ff_size, ff_data);
        // Rebuild uniform set with new buffer
        if (update_uniform_set_rid.is_valid()) rd->free_rid(update_uniform_set_rid);
        Array uniforms;
        auto add_buf = [&](int binding, RID buf, bool is_uniform) {
            Ref<RDUniform> u;
            u.instantiate();
            u->set_uniform_type(is_uniform ? RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
            u->set_binding(binding);
            u->add_id(buf);
            uniforms.push_back(u);
        };
        add_buf(0, position_buffer_rid, false);
        add_buf(1, velocity_buffer_rid, false);
        add_buf(2, color_buffer_rid, false);
        add_buf(3, update_params_rid, true);
        add_buf(4, force_field_buffer_rid, false);
        update_uniform_set_rid = rd->uniform_set_create(uniforms, update_shader_rid, 0);
        force_fields_dirty = false;
    }

    // Update params (64 bytes)
    {
        PackedByteArray params;
        params.resize(64);
        params.fill(0);

        float *f = reinterpret_cast<float *>(params.ptrw());
        int32_t *i = reinterpret_cast<int32_t *>(params.ptrw());

        f[0] = delta;
        f[1] = gravity;
        i[2] = num_particles;
        i[3] = (int32_t)force_fields.size();

        f[4] = color_start.r; f[5] = color_start.g;
        f[6] = color_start.b; f[7] = color_start.a;
        f[8] = color_end.r; f[9] = color_end.g;
        f[10] = color_end.b; f[11] = color_end.a;
        f[12] = sim_time;
        f[13] = drag;
        f[14] = 0.0f;
        f[15] = 0.0f;

        rd->buffer_update(update_params_rid, 0, 64, params);
    }

    uint32_t groups = (num_particles + 255) / 256;
    int64_t cl = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline(cl, update_pipeline_rid);
    rd->compute_list_bind_uniform_set(cl, update_uniform_set_rid, 0);
    rd->compute_list_dispatch(cl, groups, 1, 1);
    rd->compute_list_end();
    rd->submit();
    rd->sync();
}

// --- Readback and update MultiMesh ---

void TempestEmitter::_readback_and_update_multimesh() {
    RenderingDevice *rd = local_rd;
    if (!rd) return;

    PackedByteArray pos_bytes = rd->buffer_get_data(position_buffer_rid);
    PackedByteArray col_bytes = rd->buffer_get_data(color_buffer_rid);

    if (pos_bytes.size() == 0) return;

    const float *positions = reinterpret_cast<const float *>(pos_bytes.ptr());
    const float *colors_data = reinterpret_cast<const float *>(col_bytes.ptr());

    Ref<MultiMesh> mm = get_multimesh();
    if (!mm.is_valid()) return;

    // Count alive particles and update their transforms + colors
    int visible = 0;
    for (int i = 0; i < num_particles; i++) {
        float age = positions[i * 4 + 3];
        if (age < 0.0f) continue; // dead

        float px = positions[i * 4 + 0];
        float py = positions[i * 4 + 1];
        float pz = positions[i * 4 + 2];

        Transform3D xform;
        xform.origin = Vector3(px, py, pz);
        // Billboard scaling by particle_size (base mesh is 1x1 quad)
        xform.basis = Basis().scaled(Vector3(particle_size, particle_size, particle_size));

        mm->set_instance_transform(visible, xform);

        Color c(
            colors_data[i * 4 + 0],
            colors_data[i * 4 + 1],
            colors_data[i * 4 + 2],
            colors_data[i * 4 + 3]
        );
        mm->set_instance_color(visible, c);

        visible++;
    }

    mm->set_visible_instance_count(visible);
}

// --- Cleanup ---

void TempestEmitter::_cleanup_compute() {
    if (!compute_initialized || !local_rd) return;

    RenderingDevice *rd = local_rd;

    if (update_uniform_set_rid.is_valid()) rd->free_rid(update_uniform_set_rid);
    if (emit_uniform_set_rid.is_valid()) rd->free_rid(emit_uniform_set_rid);
    if (update_pipeline_rid.is_valid()) rd->free_rid(update_pipeline_rid);
    if (emit_pipeline_rid.is_valid()) rd->free_rid(emit_pipeline_rid);
    if (update_shader_rid.is_valid()) rd->free_rid(update_shader_rid);
    if (emit_shader_rid.is_valid()) rd->free_rid(emit_shader_rid);
    if (position_buffer_rid.is_valid()) rd->free_rid(position_buffer_rid);
    if (velocity_buffer_rid.is_valid()) rd->free_rid(velocity_buffer_rid);
    if (color_buffer_rid.is_valid()) rd->free_rid(color_buffer_rid);
    if (update_params_rid.is_valid()) rd->free_rid(update_params_rid);
    if (emit_params_rid.is_valid()) rd->free_rid(emit_params_rid);
    if (emit_counter_rid.is_valid()) rd->free_rid(emit_counter_rid);
    if (force_field_buffer_rid.is_valid()) rd->free_rid(force_field_buffer_rid);

    memdelete(rd);
    local_rd = nullptr;
    compute_initialized = false;
}
