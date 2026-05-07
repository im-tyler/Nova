# Project Cascade

Last updated: 2026-03-24

## Mission

Build GPU-accelerated cloth and fluid simulation systems for Godot that bring it closer to Unreal's Chaos Cloth and Niagara fluid capabilities, integrated with the engine's physics and rendering pipelines.

## Context

### The Gap

Godot has no built-in cloth physics node. The closest options are:

- **SoftBody3D** — generic soft body, not designed for cloth. Breaks down with constant collisions (skirts, garments against body).
- **Silkload addon** — community bone-driven cloth using verlet integration. Functional but CPU-only, limited collision.
- **Godot Rapier Physics** — Rust-based physics replacement with fluid support via Salva library. Best current fluid option but not GPU-accelerated.

Unreal provides:

- **Chaos Cloth** — GPU-accelerated XPBD cloth solver, vertex painting for constraints, deep integration with skeletal meshes and rendering
- **Niagara Fluids** — GPU particle system with fluid simulation capabilities, SPH/FLIP solvers, rendering integration

### Current Community State

| Solution | Type | GPU | Quality |
|---|---|---|---|
| SoftBody3D | Built-in | No | Basic deformation only |
| Silkload | Addon | No | Bone-driven cloth, functional |
| Rapier/Salva | Addon | SIMD only | Best fluid option, CPU-bound |
| Various verlet addons | Addon | No | 2D/simple 3D cloth |

None of these approach Unreal's integration or performance level.

## Product Goal

### Tier 1: GPU Cloth

A dedicated cloth simulation node for Godot with:

- GPU-accelerated XPBD or PBD solver
- vertex painting for constraint weights
- collision with physics bodies and self-collision
- integration with skeletal meshes (characters wearing cloth)
- wind and force field support
- reasonable performance for game use (not offline simulation)

### Tier 2: GPU Fluid

A fluid simulation system with:

- GPU particle-based fluid (SPH or FLIP/APIC)
- surface reconstruction for rendering
- interaction with physics bodies
- basic viscosity, surface tension controls
- suitable for small-to-medium scale game effects (not ocean simulation)

### Tier 3: Production Polish

- LOD for cloth (distance-based simplification)
- cloth/fluid interaction with Meridian's dense geometry
- Niagara-style VFX integration for fluid rendering
- deterministic mode for replays
- broad material support (wet surfaces, cloth shading models)

## Non-Goals for v1

- ocean / large-scale water simulation
- hair simulation (related but distinct problem)
- destruction / fracture physics
- soft body beyond cloth
- mobile or web GPU compute

## Technical Approach

### Cloth: XPBD on GPU Compute

Extended Position-Based Dynamics is the current standard for real-time cloth:

- well understood algorithm with good GPU parallelization
- Unreal's Chaos Cloth uses XPBD
- constraint-based: distance, bending, collision, attachment
- stable at game-appropriate time steps

Implementation path:

1. Compute shader XPBD solver
2. Constraint types: distance, bending, attachment, collision
3. Vertex painting tool for constraint weights
4. Skeletal mesh binding for character cloth
5. Wind and force field integration

### Fluid: SPH on GPU Compute

Smoothed Particle Hydrodynamics for real-time fluid:

- particle-based, naturally parallelizable on GPU
- spatial hashing for neighbor search
- pressure, viscosity, surface tension forces
- surface reconstruction via marching cubes or screen-space rendering

Implementation path:

1. GPU compute SPH solver with spatial hashing
2. Boundary handling for physics body interaction
3. Surface reconstruction for rendering
4. Basic material properties (density, viscosity)
5. Screen-space fluid rendering as fast path

## Core Decisions

### 1. GPU compute is the foundation

CPU cloth/fluid cannot compete with Unreal. GPU compute via Godot's RenderingDevice is the only path to competitive performance.

### 2. Cloth before fluid

Cloth is more commonly needed in game production, better understood algorithmically, and has a clearer integration story with existing Godot nodes (skeletal meshes, physics bodies).

### 3. XPBD for cloth solver

Not custom/novel — use the established algorithm that Unreal and most modern engines use. The value is in the integration, not the solver innovation.

### 4. Separate from Godot's physics engine

Cloth and fluid run on GPU compute, separate from Godot's CPU physics (Godot Physics or Jolt). They read collision geometry from the physics world but do their own solving.

### 5. Delivery as GDExtension initially

Unlike Meridian and Aurora, cloth/fluid simulation is less renderer-coupled. A GDExtension that uses RenderingDevice compute should be viable for the solver. Rendering integration may need deeper hooks.

## Phase Plan

### Phase 0: Research and Prototype

Duration: 4 to 6 weeks

Deliverables:

- GPU compute XPBD prototype in standalone Vulkan
- performance benchmarks vs CPU baselines
- Godot RenderingDevice compute feasibility for simulation workloads
- constraint system design
- integration point mapping (skeletal mesh, physics world, rendering)

Exit criteria:

- GPU cloth prototype runs and deforms correctly
- Godot compute shader path is validated for this workload

### Phase 1: Cloth Node

Duration: 10 to 14 weeks

Build:

- ClothBody3D node (or equivalent)
- GPU XPBD solver via RenderingDevice compute
- distance, bending, attachment constraints
- collision with physics bodies
- skeletal mesh binding
- vertex painting tool for constraints
- wind and basic force fields
- editor preview

Exit criteria:

- cloth works on characters in Godot scenes
- performance is competitive with mid-range Unreal cloth

### Phase 2: Cloth Polish

Duration: 6 to 8 weeks

Build:

- self-collision
- LOD (distance-based constraint reduction)
- constraint presets for common use cases
- stability improvements and edge case handling
- performance optimization

Exit criteria:

- cloth is production-usable for character garments and environmental cloth

### Phase 3: Fluid System

Duration: 12 to 16 weeks

Build:

- GPU SPH solver
- spatial hashing for neighbor search
- boundary handling
- surface reconstruction
- screen-space fluid rendering
- basic material properties
- physics body interaction
- FluidBody3D node (or equivalent)

Exit criteria:

- small-to-medium fluid effects render in Godot scenes
- performance is acceptable for game use

### Phase 4: Integration and Advanced

Build selectively:

- Meridian dense geometry collision
- Aurora lighting interaction (caustics, wet surfaces)
- FLIP/APIC solver upgrade for fluid
- hair simulation prototype
- VFX system integration

## Primary Risks

1. **GPU compute via GDExtension limitations** — if RenderingDevice compute access is insufficient, may need engine module
2. **Rendering integration** — getting simulated cloth/fluid to render correctly with Godot's material system may require deeper hooks than expected
3. **Skeletal mesh binding** — integrating GPU cloth with Godot's skeletal animation pipeline has unclear complexity
4. **Performance expectations** — GPU cloth/fluid is faster than CPU but still expensive; need to manage scope of simulated elements
5. **Vertex painting tooling** — editor tools for constraint painting are significant UX work

## Related Projects

- [Project Meridian](/Users/tyler/Documents/renderer/PROJECT_PLAN.md) — dense geometry (collision source for cloth/fluid)
- [Project Aurora](/Users/tyler/Documents/lighting/PROJECT_PLAN.md) — lighting (wet surface materials, caustics)

## Sources

- Godot cloth proposal: https://github.com/godotengine/godot-proposals/issues/2513
- Godot cloth/hair proposal: https://github.com/godotengine/godot-proposals/issues/2833
- Godot GPU physics discussion: https://github.com/godotengine/godot/issues/22448
- Godot Silkload: https://godotengine.org/asset-library/asset/3785
- Godot Rapier Physics: https://github.com/appsinacup/godot-rapier-physics
- Godot Rapier fluid docs: https://godot.rapier.rs/docs/tutorial/create-a-fluid/
- Godot SoftBody3D docs: https://docs.godotengine.org/en/stable/tutorials/physics/soft_body.html
- Blast SDK: https://github.com/NVIDIAGameWorks/Blast
- Blast SDK docs: https://nvidia-omniverse.github.io/PhysX/blast/index.html
- PhysX Flow SDK: https://developer.nvidia.com/physx-sdk
- WebGPU XPBD cloth reference: https://github.com/jspdown/cloth
- Lighthugger (Meridian reference for collision geometry): https://github.com/expenses/lighthugger
