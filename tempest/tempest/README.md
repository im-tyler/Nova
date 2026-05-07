# Project Tempest

GPU particle system for Godot 4.4+, built as a GDExtension (C++ / godot-cpp). Tempest runs emit and update logic entirely on the GPU via compute shaders dispatched through Godot's RenderingDevice API. Particles are rendered using MultiMesh instancing.

Part of the Godot-Unreal Parity Initiative.

## Node Types

| Node | Base Class | Purpose |
|------|-----------|---------|
| `TempestEmitter` | MultiMeshInstance3D | GPU-driven particle emitter. Emits, updates, and renders particles each frame. |

## Build

Requires: SCons, a C++ compiler with C++17 support, Python 3. Tempest reuses Cascade's godot-cpp checkout via a relative symlink.

```bash
cd tempest/tempest
# Re-create the godot-cpp symlink if needed (points at Cascade's checkout):
ln -sf ../../cascade/cascade/godot-cpp godot-cpp
# Make sure godot-cpp is built (Cascade's instructions cover this):
cd godot-cpp && scons platform=macos target=template_debug -j10 && cd ..
scons platform=macos target=template_debug -j10
```

Output:

```
bin/libtempest.macos.template_debug.framework
```

Replace `macos` / `template_debug` as needed for other platforms and release builds.

## Install

1. Copy the `bin/` directory into your Godot project root.
2. Copy `tempest.gdextension` into your Godot project root.
3. Open the project in Godot 4.4+. `TempestEmitter` will appear in the Add Node dialog.

```
your_project/
  project.godot
  tempest.gdextension
  bin/
    libtempest.macos.template_debug.framework
```

## Usage

1. Add a `TempestEmitter` node to your scene.
2. Configure properties in the inspector:

| Property | Default | Description |
|----------|---------|-------------|
| `num_particles` | 4096 | Maximum particle pool size |
| `emission_rate` | 500.0 | Particles emitted per second |
| `lifetime` | 3.0 | Seconds before a particle dies |
| `gravity` | 9.8 | Downward acceleration (m/s^2) |
| `initial_velocity` | (0, 8, 0) | Base emission velocity |
| `spread_angle` | 0.5 | Emission cone half-angle in radians |
| `emission_shape` | POINT | Emission shape: POINT, SPHERE, or BOX |
| `particle_size` | 0.05 | Visual size of each particle sphere |
| `color_start` | (1, 0.8, 0.2, 1) | Color at birth (RGBA) |
| `color_end` | (1, 0.1, 0, 0) | Color at death (RGBA, alpha fades out) |
| `emitting` | true | Whether the emitter is active |

3. Set `emitting = true` to start. Particles will emit and simulate immediately.

### Script Control

```gdscript
var emitter = $TempestEmitter
emitter.num_particles = 8192
emitter.emission_rate = 1000.0
emitter.initial_velocity = Vector3(0, 12, 0)
emitter.color_start = Color(0.2, 0.5, 1.0, 1.0)
emitter.color_end = Color(0.0, 0.0, 0.5, 0.0)
emitter.emitting = true
```

## Architecture

TempestEmitter runs two compute shaders per frame:

1. **Emit shader** -- initializes new particles with randomized position (based on emission shape), velocity (spread cone), lifetime, and color.
2. **Update shader** -- advances particle positions by velocity, applies gravity, ages particles, interpolates color from start to end over lifetime, kills expired particles.

GPU buffers store per-particle data as vec4 arrays:
- Position buffer: xyz = position, w = age
- Velocity buffer: xyz = velocity, w = lifetime
- Color buffer: rgba

After the update dispatch, positions and colors are read back to update the MultiMesh instance transforms and are visible immediately.

## Current Limitations

- **Basic emit/update only.** The system supports a single emitter dispatching to one particle pool. No sub-emitters or particle events.
- **No force fields.** Only gravity is applied. No attractors, wind zones, turbulence, curl noise, or collision with scene geometry.
- **No VFX graph.** Behavior is configured via properties, not a visual node graph. A graph-based system is planned.
- **CPU readback.** MultiMesh transforms are updated via CPU readback from the compute buffers each frame.
- **No sorting.** Particles are not depth-sorted for correct alpha blending.
- **Sphere-only rendering.** Particles render as instanced spheres. No billboard quads, trails, or mesh particles.
