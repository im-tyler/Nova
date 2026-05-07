# Project Tempest

GPU-driven particle / VFX system for Godot — Niagara as the parity bar. C++ GDExtension running emit and update on the GPU via compute shaders dispatched through `RenderingDevice`. Particles render via `MultiMesh` instancing.

**Status: working prototype.** A single `TempestEmitter` node type is functional: GPU emit, GPU update, gravity, color over lifetime. Most of the planned scope (force fields, sub-emitters, VFX graph, fluid/volume rendering) is not yet implemented.

## Layout

```
tempest/
  tempest/             -- the GDExtension (C++, godot-cpp + RenderingDevice)
    src/               -- TempestEmitter implementation + compute shader strings
    tempest.gdextension
    SConstruct
    README.md          -- build, install, properties, usage
  test-project/        -- Godot test project for the GDExtension
  _pre-consolidation/  -- original PROJECT_PLAN
```

The GDExtension's own README ([`tempest/README.md`](./tempest/README.md)) documents build, install, and the `TempestEmitter` properties.

## Concept

Godot's `GPUParticles3D` is functional for basic effects but lacks Niagara-class capabilities — no programmable behavior modules, no node-based VFX graph, no fluid surface reconstruction, no volume rendering for smoke/fire, limited force fields and physics integration.

Tempest sits at the intersection of rendering and physics:

- **cascade** produces simulation data (fluid particles, debris). Tempest renders it.
- **meridian** provides the rendering pipeline tempest draws into.
- **aurora** provides lighting that interacts with volumetric effects.

Without tempest, cascade's fluid and destruction output has no visual payoff beyond raw mesh deformation.

## Plan

### Tier 1 — Programmable GPU Particles
GPU compute particle system with scriptable behavior modules; emitter types (point, mesh surface, volume, ring, trail); force fields (gravity, wind, vortex, noise, attractor, drag); collision with scene geometry; sub-emitters (spawn on collision/death/event); LOD and culling; basic VFX graph editor.

### Tier 2 — Fluid and Volume Rendering
Surface reconstruction from cascade's fluid output (screen-space fast path, marching cubes quality path); volume rendering for smoke / fire / fog (Flow-style sparse-grid simulation reimplemented in GLSL compute); spray, foam, bubble secondary particles.

### Tier 3 — Production Polish
Full node-based VFX graph with editor preview, mesh particles, ribbon/trail renderers, audio-reactive particles, aurora lighting integration (emissive, volumetric scattering), LOD and performance budgeting.

## Architecture

```
VFX Graph (Godot GraphEdit)
    -> Module Chain (ordered compute snippets)
    -> Generated GLSL compute shader
    -> Compiled to SPIR-V -> dispatched via RenderingDevice
```

This mirrors Godot's `VisualShader` pipeline — graph nodes -> generated GLSL -> SPIR-V — but for compute instead of fragment/vertex.

### Shared Particle Buffer

Cascade (physics solver) and Tempest (VFX renderer) plan to share GPU buffers. Cascade writes solver output; tempest reads for rendering. Same buffer, no copy.

```glsl
struct Particle {
    vec3 position;
    float age;
    vec3 velocity;
    float lifetime;
    uint flags;
    uint material_id;
    vec2 uv;
    // custom attributes appended per-emitter
};
```

### Volume Rendering

Based on PhysX Flow SDK's sparse-grid approach, reimplemented in GLSL compute:

1. Simulation — advect density / temperature on a sparse 3D grid (compute).
2. Rendering — ray-march through the density grid (fragment).
3. Lighting — sample aurora's lighting data for volumetric scattering.

Sparse grids only allocate cells where density is non-zero, making large-volume effects memory-efficient.

## Quick Status — What Builds and Runs Today

| Node | Base | Working |
|------|------|---------|
| `TempestEmitter` | `MultiMeshInstance3D` | GPU emit + update via compute shaders, gravity, color over lifetime, three emission shapes (point/sphere/box). |

Nothing else is implemented yet. See [`tempest/README.md`](./tempest/README.md) for the full property list and current limitations.

## Phase 0 Status

- [x] Study Godot `VisualShader` graph-to-GLSL compilation.
- [ ] Study PhysX Flow SDK sparse-grid architecture.
- [x] Prototype GPU particle update via `RenderingDevice` compute (4096 particles, gravity).
- [x] Prototype GPU particle rendering (instanced spheres).
- [ ] Prototype module system (gravity, noise force, age/death as separate composable modules).
- [ ] Prototype screen-space fluid rendering from particle positions.
- [ ] Define shared particle buffer format with cascade.
- [ ] Benchmark against Godot `GPUParticles3D`.

Exit criteria: GPU particles update and render via compute shaders (done); module-chain concept validated (pending); shared buffer format agreed with cascade (pending).

## Current Limitations

- **Basic emit/update only.** Single emitter, one particle pool. No sub-emitters or particle events.
- **No force fields.** Only gravity. No attractors, wind zones, turbulence, curl noise, scene-geometry collision.
- **No VFX graph.** Behavior is configured via inspector properties, not a visual node graph.
- **CPU readback for MultiMesh transforms.** Read back from compute buffers each frame — same bottleneck pattern as cascade.
- **No depth sort.** Particles aren't sorted; alpha blending is best-effort.
- **Sphere-only rendering.** Instanced spheres only — no billboard quads, trails, or mesh particles.
- **Hard-coded shaders.** Module composition is not in place yet.

## References

- Niagara — public GDC talks and docs.
- PhysX Flow SDK (BSD-3): https://developer.nvidia.com/physx-sdk
- Godot `VisualShader` source — graph-to-GLSL compilation pattern.
- Godot `GraphEdit`: https://docs.godotengine.org/en/stable/classes/class_graphedit.html
- Screen-space fluid rendering — Simon Green, GDC.
- Godot `GPUParticles3D` source — current built-in system.
- PopcornFX — commercial reference for VFX middleware.
- WebGPU compute examples: https://github.com/scttfrdmn/webgpu-compute-exploration
