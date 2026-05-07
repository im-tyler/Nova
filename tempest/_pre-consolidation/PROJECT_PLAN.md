# Project Tempest

Last updated: 2026-03-24

## Mission

Build a GPU-driven visual effects and particle system for Godot that competes with Unreal's Niagara, delivered as a GDExtension.

## Context

### The Gap

Godot's GPUParticles3D is functional for basic effects but lacks:

- programmable/scriptable particle behavior (Niagara's module system)
- node-based VFX graph editor
- fluid rendering (surface reconstruction, spray, foam)
- volume rendering for smoke and fire
- mesh particle spawning at scale
- event-driven particle spawning (collision, death, etc.)
- force field variety and composability
- integration with physics simulation output

Niagara is arguably the most impactful artist-facing system in Unreal after the renderer itself. Every shipped game uses it.

### Relationship to Other Projects

Tempest sits at the intersection of rendering and physics:

- **Cascade** produces simulation data (fluid particle positions, debris trajectories). Tempest renders it.
- **Meridian** provides the rendering pipeline that Tempest draws into.
- **Aurora** provides lighting that interacts with volumetric effects.

Without Tempest, Cascade's fluid and destruction output has no visual payoff beyond raw mesh deformation.

## Foundations Available

- **PhysX Flow SDK (BSD-3)** -- sparse grid GPU volume simulation architecture. CUDA-based but architecture is the reference for reimplementation in GLSL compute. Covers combustible fluid, smoke, fire.
- **Godot GraphEdit** -- existing node graph UI control used by VisualShader and AnimationTree. This is the UI framework for the VFX graph editor.
- **Godot VisualShader** -- existing graph-to-GLSL compilation system. The compilation model (visual graph nodes -> generated shader code) is exactly the pattern Tempest's VFX graph uses for compute shaders.
- No open-source Niagara equivalent exists. The particle system and VFX graph are original work.

## Product Goal

### Tier 1: Programmable GPU Particles

- GPU compute particle system with scriptable behavior modules
- emitter types: point, mesh surface, volume, ring, trail
- force fields: gravity, wind, vortex, noise, attractor, drag
- collision with scene geometry
- sub-emitters (spawn on collision, death, event)
- LOD and culling for particle systems
- basic VFX graph editor for authoring

### Tier 2: Fluid and Volume Rendering

- surface reconstruction from Cascade's fluid solver output
- screen-space fluid rendering (fast path)
- marching cubes surface reconstruction (quality path)
- volume rendering for smoke, fire, fog (consumes Flow-style data or simple procedural volumes)
- spray, foam, bubble secondary particles

### Tier 3: Production Polish

- full node-based VFX graph with preview
- mesh particles (spawn full meshes as particles)
- ribbon/trail renderers
- audio-reactive particles
- integration with Aurora's lighting (emissive particles, volumetric scattering)
- LOD and performance budgeting

## Technical Approach

### GPU Compute Particle System

Core architecture:

- particle state lives in GPU buffers (position, velocity, age, custom attributes)
- update pass: compute shader dispatches per-emitter modules (forces, collision, spawning, death)
- sort pass: for transparency, depth-sort or use OIT
- render pass: point sprites, billboards, mesh instances, or trails
- all dispatched via RenderingDevice compute from GDExtension

### Module System (Niagara-equivalent)

Each particle behavior is a composable module:

- modules are compute shader snippets that read/write particle attributes
- emitter assembles modules into an update pipeline
- VFX graph editor generates the module chain visually
- custom modules writable in GLSL or via graph nodes

### Module Compilation Model

Each VFX module is a compute shader snippet. The VFX graph compiles to a combined GLSL compute shader:

```
VFX Graph (Godot GraphEdit)
    |
    v
Module Chain (ordered list of compute snippets)
    |
    v
Generated GLSL compute shader
    |
    v
Compiled to SPIR-V -> dispatched via RenderingDevice
```

This mirrors Godot's VisualShader pipeline:
- VisualShader: graph nodes -> generated fragment/vertex GLSL -> SPIR-V
- Tempest: graph nodes -> generated compute GLSL -> SPIR-V

Study Godot's VisualShader source code for the graph-to-GLSL compilation pattern.

### Shared Particle Buffer Format

Cascade (physics solver) and Tempest (VFX renderer) share GPU buffers. Cascade writes solver output; Tempest reads it for rendering. Same GPU buffer, no copy.

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

The buffer is allocated through a shared buffer registry (common infrastructure across all GPU projects).

### Fluid Rendering

Two paths:

- **Screen-space**: render particles as spheres, smooth depth buffer, shade as fluid surface. Fast, good for small-medium effects.
- **Marching cubes**: reconstruct explicit surface from particle positions. Higher quality, more expensive. Good for hero fluid.

### Volume Rendering Architecture

For smoke, fire, and fog effects. Based on Flow SDK's sparse grid approach, reimplemented in GLSL compute:

1. Simulation: advect density/temperature on a sparse 3D grid (compute shader)
2. Rendering: ray-march through the density grid (fragment shader)
3. Lighting: sample Aurora's lighting data for volumetric scattering

Sparse grids only allocate cells where density is non-zero, making large-volume effects memory-efficient.

## Delivery

GDExtension via RenderingDevice compute. The particle system doesn't need to replace Godot's built-in GPUParticles3D — it runs alongside it as an alternative for users who need more power.

## Phase Plan

### Phase 0: Research and Prototype (4-6 weeks)

- [ ] study Godot VisualShader graph-to-GLSL compilation source code
- [ ] study PhysX Flow SDK sparse grid architecture
- [ ] prototype GPU particle update via RenderingDevice compute (100K particles, basic forces)
- [ ] prototype GPU particle rendering (instanced billboards)
- [ ] prototype module system (2-3 hardcoded modules: gravity, noise force, age/death)
- [ ] prototype screen-space fluid rendering from particle positions
- [ ] define shared particle buffer format with Cascade
- [ ] benchmark against Godot GPUParticles3D

Exit criteria:
- GPU particles update and render via compute shaders
- module chain concept validated
- shared buffer format agreed with Cascade

### Phase 1: Core Particle System

- emitters, forces, collision, sub-emitters
- billboard and point sprite rendering
- basic editor UI for authoring
- GDExtension delivery

### Phase 2: VFX Graph Editor

- node-based visual editor for module composition
- preview in editor
- custom attribute system
- event system (spawn on collision, death, etc.)

### Phase 3: Fluid and Volume Rendering

- screen-space fluid rendering
- marching cubes surface reconstruction
- volume rendering for smoke/fire
- integration with Cascade solver output

### Phase 4: Advanced

- mesh particles
- ribbon/trail renderers
- Aurora lighting integration
- performance budgeting and LOD
- audio-reactive modules

## Key References

- Niagara overview (public GDC talks and docs)
- PhysX Flow SDK (BSD-3): https://developer.nvidia.com/physx-sdk
- Godot VisualShader source: study graph-to-GLSL compilation pattern
- Godot GraphEdit docs: https://docs.godotengine.org/en/stable/classes/class_graphedit.html
- Screen-space fluid rendering (Simon Green, GDC)
- Godot GPUParticles3D source (understand current system)
- PopcornFX (commercial reference for VFX middleware)
- WebGPU compute examples: https://github.com/scttfrdmn/webgpu-compute-exploration
