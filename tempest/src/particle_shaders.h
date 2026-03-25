#pragma once

// Compute shader: per-particle update (apply forces, age, death, color interpolation).
// Reads and writes particle state buffers in-place.
//
// Buffer layout per particle (3 vec4s = 48 bytes):
//   vec4(position.xyz, age)
//   vec4(velocity.xyz, lifetime)
//   vec4(color.rgba)
static const char *PARTICLE_UPDATE_SHADER = R"(
#version 450

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict buffer ParticlePositions {
    vec4 positions[];  // xyz = position, w = age
};

layout(set = 0, binding = 1, std430) restrict buffer ParticleVelocities {
    vec4 velocities[];  // xyz = velocity, w = lifetime
};

layout(set = 0, binding = 2, std430) restrict buffer ParticleColors {
    vec4 colors[];  // rgba
};

layout(set = 0, binding = 3, std140) uniform Params {
    float delta_time;
    float gravity;
    int num_particles;
    int num_force_fields;
    vec4 color_start;
    vec4 color_end;
    float time;
    float drag;
    float pad1;
    float pad2;
};

// Force field buffer: each field is 3 vec4s:
//   vec4(type, strength, radius, falloff)
//   vec4(position.xyz, 0)
//   vec4(direction.xyz, frequency)  -- direction for vortex axis, frequency for turbulence
// Types: 0=attractor, 1=repulsor, 2=vortex, 3=turbulence, 4=directional_wind
layout(set = 0, binding = 4, std430) restrict readonly buffer ForceFields {
    vec4 force_fields[];  // 3 vec4s per field
};

// Simple 3D noise for turbulence
float hash31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

vec3 noise3(vec3 p) {
    return vec3(
        hash31(p) * 2.0 - 1.0,
        hash31(p + vec3(31.5, 17.3, 7.7)) * 2.0 - 1.0,
        hash31(p + vec3(61.2, 43.1, 29.9)) * 2.0 - 1.0
    );
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(num_particles)) return;

    vec4 pos_age = positions[idx];
    vec4 vel_life = velocities[idx];

    float age = pos_age.w;
    float lifetime = vel_life.w;

    if (age < 0.0) return;

    age += delta_time;

    if (age > lifetime) {
        positions[idx].w = -1.0;
        return;
    }

    vec3 vel = vel_life.xyz;
    vec3 pos = pos_age.xyz;

    // Gravity
    vel.y -= gravity * delta_time;

    // Force fields
    for (int f = 0; f < num_force_fields; f++) {
        vec4 ff_params = force_fields[f * 3 + 0];
        vec4 ff_pos    = force_fields[f * 3 + 1];
        vec4 ff_dir    = force_fields[f * 3 + 2];

        int ff_type      = int(ff_params.x);
        float strength   = ff_params.y;
        float radius     = ff_params.z;
        float falloff    = ff_params.w;

        vec3 to_field = ff_pos.xyz - pos;
        float dist = length(to_field);

        // Falloff: 1.0 at center, 0.0 at radius
        float atten = 1.0;
        if (radius > 0.0 && dist > 0.0) {
            atten = pow(max(0.0, 1.0 - dist / radius), falloff);
        }

        if (ff_type == 0) {
            // Attractor: pull toward position
            if (dist > 0.001) {
                vel += normalize(to_field) * strength * atten * delta_time;
            }
        } else if (ff_type == 1) {
            // Repulsor: push away from position
            if (dist > 0.001) {
                vel -= normalize(to_field) * strength * atten * delta_time;
            }
        } else if (ff_type == 2) {
            // Vortex: swirl around axis defined by ff_dir.xyz
            vec3 axis = normalize(ff_dir.xyz);
            vec3 radial = to_field - dot(to_field, axis) * axis;
            vec3 tangent = cross(axis, radial);
            if (length(tangent) > 0.001) {
                vel += normalize(tangent) * strength * atten * delta_time;
            }
        } else if (ff_type == 3) {
            // Turbulence: noise-based random force
            float freq = max(ff_dir.w, 1.0);
            vec3 n = noise3(pos * freq + vec3(time * 2.0));
            vel += n * strength * atten * delta_time;
        } else if (ff_type == 4) {
            // Directional wind
            vel += ff_dir.xyz * strength * atten * delta_time;
        }
    }

    // Drag
    vel *= (1.0 - drag * delta_time);

    // Integrate
    pos += vel * delta_time;

    positions[idx] = vec4(pos, age);
    velocities[idx] = vec4(vel, lifetime);

    float t = clamp(age / lifetime, 0.0, 1.0);
    colors[idx] = mix(color_start, color_end, t);
}
)";

// Compute shader: emit new particles from emitter shape.
// Scans for dead particles (age < 0) and spawns them.
// Uses a simple pseudo-random hash for distribution.
//
// emission_shape: 0 = point, 1 = sphere, 2 = box
static const char *PARTICLE_EMIT_SHADER = R"(
#version 450

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict buffer ParticlePositions {
    vec4 positions[];  // xyz = position, w = age
};

layout(set = 0, binding = 1, std430) restrict buffer ParticleVelocities {
    vec4 velocities[];  // xyz = velocity, w = lifetime
};

layout(set = 0, binding = 2, std430) restrict buffer ParticleColors {
    vec4 colors[];  // rgba
};

layout(set = 0, binding = 3, std430) restrict buffer EmitCounter {
    int emit_count;  // how many particles to emit this frame
};

layout(set = 0, binding = 4, std140) uniform EmitParams {
    vec4 emitter_position;    // xyz = position, w = unused
    vec4 initial_velocity;    // xyz = base velocity, w = spread_angle (radians)
    vec4 color_start;
    float lifetime;
    float particle_size;
    int emission_shape;       // 0=point, 1=sphere, 2=box
    float shape_radius;       // radius for sphere, half-extent for box
    uint frame_seed;          // changes each frame for randomness
    int num_particles;
    float pad0;
    float pad1;
};

// Simple hash for pseudo-random numbers on GPU
uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand_float(uint seed) {
    return float(pcg_hash(seed)) / 4294967295.0;
}

vec3 rand_direction(uint seed) {
    float theta = rand_float(seed) * 6.28318530718;
    float z = rand_float(seed + 1u) * 2.0 - 1.0;
    float r = sqrt(1.0 - z * z);
    return vec3(r * cos(theta), r * sin(theta), z);
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(num_particles)) return;

    // Only process dead particles
    if (positions[idx].w >= 0.0) return;

    // Atomically decrement emit counter; if we get a positive value, we spawn
    int slot = atomicAdd(emit_count, -1);
    if (slot <= 0) {
        // No more emissions this frame, restore counter
        atomicAdd(emit_count, 1);
        return;
    }

    // Seed unique to this particle and frame
    uint seed = idx * 1973u + frame_seed * 6971u;

    // Compute spawn position based on emission shape
    vec3 spawn_pos = emitter_position.xyz;

    if (emission_shape == 1) {
        // Sphere: random point inside sphere
        vec3 dir = rand_direction(seed + 100u);
        float dist = pow(rand_float(seed + 200u), 1.0 / 3.0) * shape_radius;
        spawn_pos += dir * dist;
    } else if (emission_shape == 2) {
        // Box: random point inside box
        float bx = (rand_float(seed + 300u) * 2.0 - 1.0) * shape_radius;
        float by = (rand_float(seed + 400u) * 2.0 - 1.0) * shape_radius;
        float bz = (rand_float(seed + 500u) * 2.0 - 1.0) * shape_radius;
        spawn_pos += vec3(bx, by, bz);
    }
    // shape 0 (point): spawn_pos stays at emitter_position

    // Compute initial velocity with spread
    float spread = initial_velocity.w;
    vec3 base_vel = initial_velocity.xyz;

    if (spread > 0.0) {
        vec3 rand_dir = rand_direction(seed + 600u);
        // Mix the base direction with a random direction based on spread
        vec3 base_norm = length(base_vel) > 0.001 ? normalize(base_vel) : vec3(0.0, 1.0, 0.0);
        vec3 spread_dir = normalize(mix(base_norm, rand_dir, spread / 3.14159));
        base_vel = spread_dir * length(base_vel);
    }

    // Write particle state
    positions[idx] = vec4(spawn_pos, 0.0);  // age = 0
    velocities[idx] = vec4(base_vel, lifetime);
    colors[idx] = color_start;
}
)";
