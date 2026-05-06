#pragma once

#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>

#include <vector>

namespace godot {

class TempestEmitter : public MultiMeshInstance3D {
    GDCLASS(TempestEmitter, MultiMeshInstance3D)

public:
    enum EmissionShape {
        EMISSION_SHAPE_POINT = 0,
        EMISSION_SHAPE_SPHERE = 1,
        EMISSION_SHAPE_BOX = 2,
    };

    TempestEmitter();
    ~TempestEmitter();

    void _ready() override;
    void _process(double delta) override;

    // Property accessors
    void set_num_particles(int p_num);
    int get_num_particles() const;

    void set_emission_rate(float p_rate);
    float get_emission_rate() const;

    void set_lifetime(float p_lifetime);
    float get_lifetime() const;

    void set_gravity(float p_gravity);
    float get_gravity() const;

    void set_initial_velocity(Vector3 p_vel);
    Vector3 get_initial_velocity() const;

    void set_spread_angle(float p_angle);
    float get_spread_angle() const;

    void set_emission_shape(int p_shape);
    int get_emission_shape() const;

    void set_particle_size(float p_size);
    float get_particle_size() const;

    void set_color_start(Color p_color);
    Color get_color_start() const;

    void set_color_end(Color p_color);
    Color get_color_end() const;

    void set_emitting(bool p_emitting);
    bool get_emitting() const;

    void set_drag(float p_drag);
    float get_drag() const;

    // Force fields: type (0=attractor,1=repulsor,2=vortex,3=turbulence,4=wind), position, direction, strength, radius, falloff
    void add_attractor(Vector3 position, float strength, float radius, float falloff);
    void add_repulsor(Vector3 position, float strength, float radius, float falloff);
    void add_vortex(Vector3 position, Vector3 axis, float strength, float radius, float falloff);
    void add_turbulence(Vector3 position, float strength, float radius, float frequency);
    void add_wind(Vector3 direction, float strength, float radius);
    void clear_force_fields();

protected:
    static void _bind_methods();

private:
    void _init_compute();
    void _init_multimesh();
    void _dispatch_emit(int count);
    void _dispatch_update(float delta);
    void _readback_and_update_multimesh();
    void _cleanup_compute();

    // Emitter properties
    int num_particles = 4096;
    float emission_rate = 500.0f;
    float lifetime = 3.0f;
    float gravity = 9.8f;
    Vector3 initial_velocity = Vector3(0.0f, 8.0f, 0.0f);
    float spread_angle = 0.5f;  // radians
    int emission_shape = EMISSION_SHAPE_POINT;
    float particle_size = 0.05f;
    Color color_start = Color(1.0f, 0.8f, 0.2f, 1.0f);
    Color color_end = Color(1.0f, 0.1f, 0.0f, 0.0f);
    bool emitting = true;
    float drag = 0.1f;

    // Force field data: 3 vec4s per field (params, position, direction)
    struct ForceField {
        float type, strength, radius, falloff;
        float px, py, pz, pad1;
        float dx, dy, dz, frequency;
    };
    std::vector<ForceField> force_fields;
    bool force_fields_dirty = true;

    // Emit accumulator (fractional particles across frames)
    float emit_accumulator = 0.0f;
    float sim_time = 0.0f;
    uint32_t frame_counter = 0;

    // Local RenderingDevice
    RenderingDevice *local_rd = nullptr;

    // Update shader resources
    RID update_shader_rid;
    RID update_pipeline_rid;
    RID update_uniform_set_rid;

    // Emit shader resources
    RID emit_shader_rid;
    RID emit_pipeline_rid;
    RID emit_uniform_set_rid;

    // Shared GPU buffers
    RID position_buffer_rid;   // vec4 per particle (xyz=pos, w=age)
    RID velocity_buffer_rid;   // vec4 per particle (xyz=vel, w=lifetime)
    RID color_buffer_rid;      // vec4 per particle (rgba)

    // Per-shader uniform buffers
    RID update_params_rid;
    RID emit_params_rid;
    RID emit_counter_rid;
    RID force_field_buffer_rid;

    bool compute_initialized = false;
};

}

VARIANT_ENUM_CAST(TempestEmitter::EmissionShape);
