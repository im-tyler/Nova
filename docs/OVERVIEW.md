# Godot-Unreal Parity Initiative

Last updated: 2026-03-24

## Mission

Close the critical feature gaps between Godot and Unreal Engine through a set of focused, interconnected projects that bring production-grade rendering, lighting, simulation, and world-scale capabilities to Godot.

## System Categories

### Category 1: Rendering

**Project Meridian** — `/Users/tyler/Documents/renderer/`

Status: Phase 0 (feasibility and baseline). Active development in separate chat.

Scope:

- dense geometry / virtualized geometry (Nanite equivalent)
- visibility buffer rendering
- GPU-driven culling + LOD hierarchy
- geometry page streaming with bounded memory
- virtual shadow maps (dense-geometry-aware shadows)
- material resolve (constrained PBR subset)

This is the foundation. Everything else draws on top of what the renderer produces.

### Category 2: Lighting

**Project Aurora** — `/Users/tyler/Documents/lighting/`

Status: Phase 0 (NVIDIA RTX fork assessment)

Scope:

- global illumination / path tracing (Lumen equivalent)
- vendor-agnostic denoiser
- hybrid GI fallback (probes, screen-space) for non-RT hardware
- reflections
- volumetric lighting and fog (later phase)

Decisions finalized:

- Foundation: NVIDIA RTX Godot fork (MIT, released GDC March 2026)
- Denoiser: Intel OIDN (Apache 2.0, multi-vendor GPU, Vulkan buffer sharing)
- OIDN 3 (H2 2026) adds temporal denoising for real-time flicker reduction
- Aurora's BVH construction consumes Meridian's cluster hierarchy data
- Consumes shadow data from Meridian

### Category 3: Physics Simulation

**Project Cascade** — `/Users/tyler/Documents/physics-sim/`

Status: Phase 0 (GPU compute prototype)

Scope:

- cloth simulation (GPU XPBD solver)
- fluid simulation (GPU SPH/FLIP solver)
- destruction / fracture (Blast-style, Voronoi)
- soft body / deformable

Decisions finalized:

- C++ with Godot RenderingDevice (Vulkan compute), GLSL shaders compilable to SPIR-V
- PhysX 5.6 (BSD-3) as algorithmic reference, not runtime dependency
- Blast SDK (BSD-3) integrated directly for destruction (physics/graphics agnostic)
- SPH for fluid (initial), FLIP/APIC later
- No CUDA/vendor lock-in — all Vulkan compute

Cascade owns the **simulation math**. It does not own the visual rendering of effects (that's VFX).

### Category 4: VFX / Particles

**Project Tempest** — `/Users/tyler/Documents/vfx/`

Status: Tier 2 — planned, not started. Blocked on Cascade (needs solver output) and Meridian (needs rendering pipeline).

Foundations: PhysX Flow SDK (BSD-3) for volume rendering architecture. Godot VisualShader as compilation pattern (graph → GLSL). Godot GraphEdit for VFX graph editor. No open-source Niagara equivalent exists — original work.

Scope:

- GPU particle system (Niagara equivalent)
- VFX graph editor compiling to GLSL compute via VisualShader pattern
- fluid rendering (surface reconstruction, spray, foam) — consumes Cascade solver output via shared particle buffer
- smoke / fire volume rendering via sparse grid (Flow SDK architecture)
- debris / sparks / trails
- force fields, attractors, emitters

### Category 5: World Streaming

**Project Atlas** — `/Users/tyler/Documents/world-streaming/`

Status: Tier 2 — planned, not started. Blocked on Meridian (needs geometry streaming to coordinate).

Foundations: OWDB Godot addon (streaming patterns reference). Godot large world coordinates already solved (double precision build). Godot ResourceLoader async loading already exists. Atlas is a coordination/policy layer, not low-level I/O.

Scope:

- world partition / level streaming (Unreal World Partition equivalent)
- data layers for world state
- coordinates geometry streaming (Meridian), texture streaming, and asset loading at world scale
- large-world coordinate support

Key relationship: Meridian streams geometry pages. World streaming is the higher-level system that decides *which parts of the world* to load. Meridian is a consumer of world streaming decisions.

### Category 6: Animation

**Project Kinetic** — `/Users/tyler/Documents/animation/`

Status: Tier 3 — planned, not started. Independent of other projects.

Foundations: orangeduck/Motion-Matching (canonical C++ reference by inventor of Learned Motion Matching). Open-Source-Motion-Matching-System (Unreal sample rewrite). SIGGRAPH Asia 2025 environment-aware motion matching. Mesh2Motion (open source auto-rigging).

Scope:

- procedural animation system (Control Rig equivalent)
- advanced IK solvers
- motion matching
- runtime retargeting
- physics-driven animation

### Category 7: Audio

**Project Resonance** — `/Users/tyler/Documents/audio/`

Status: Tier 3 — planned, not started. Independent of other projects.

Foundations: Steam Audio (free SDK, Godot GDExtension ports already exist) for spatial audio. LabSound (BSD-2, C++ graph-based audio engine) for programmable audio graph. Two-layer architecture: Steam Audio handles spatial, LabSound handles DSP graph.

Scope:

- programmable audio graph (MetaSounds equivalent) built on LabSound
- spatial audio via Steam Audio (already has Godot integration)
- node-based audio graph editor via Godot GraphEdit

### Category 8: Procedural Generation

**Project Scatter** — `/Users/tyler/Documents/procgen/`

Status: Tier 3 — planned, not started. Benefits from Atlas but independent.

Foundations: No open-source PCG framework exists — original work. Godot GraphEdit for graph editor. Godot MultiMeshInstance3D for instanced output. Unreal 5.7 PCG Framework docs as architecture reference. Simplest project: pure EditorPlugin, no GPU compute needed.

Scope:

- node-based procedural placement and generation (PCG Framework equivalent)
- point-set-operation graph model (sample → filter → transform → instance)
- rule-based scattering (rocks, foliage, props)
- editor-time and runtime execution

## Dependency Map

```
Rendering (Meridian)
    |
    +---> Lighting (Aurora)
    |         - needs geometry for BVH
    |         - consumes shadow data from Meridian
    |
    +---> VFX / Particles
    |         - uses renderer to draw effects
    |         - needs rendering pipeline hooks
    |
    +---> World Streaming
              - coordinates Meridian's geometry streaming
              - decides what to load at world scale

Physics Sim (Cascade)
    |
    +---> VFX / Particles
    |         - fluid rendering consumes solver output
    |         - destruction VFX consumes debris data
    |
    +---> Animation
              - cloth interacts with skeletal animation
              - physics-driven animation reads sim state

Lighting (Aurora)
    |
    +---> VFX / Particles
              - volumetric lighting interacts with smoke/fog
              - emissive particles feed into path tracer
```

Independent categories (no hard dependencies):

- Audio
- Procedural Generation (benefits from World Streaming but doesn't require it)

## Priority Order

### Tier 1: Foundation (current focus)

1. **Rendering (Meridian)** — everything builds on this
2. **Lighting (Aurora)** — the biggest visual quality leap
3. **Physics Sim (Cascade)** — cloth/fluid/destruction

### Tier 2: Production completeness

4. **VFX / Particles** — makes physics visible, handles all visual effects
5. **World Streaming** — makes rendering and lighting work at scale

### Tier 3: Polish and breadth

6. **Animation** — character quality and procedural motion
7. **Procedural Generation** — content creation efficiency
8. **Audio** — programmable sound design

## Shared Infrastructure

All GPU-heavy projects share:

- **Vulkan compute patterns** — dispatch, buffer management, synchronization
- **Godot RenderingDevice** — common compute API surface
- **Shader portability** — GLSL compute → SPIR-V → cross-compile to WGSL/MSL via SPIRV-Cross or Naga
- **Delivery vehicle** — Meridian and Aurora likely require engine module/fork; Cascade and VFX may stay GDExtension
- **Benchmark methodology** — shared hardware profiles and testing approach

## Delivery Strategy

### Guiding Principle

**Maximize GDExtension. Minimize engine patches. Every patch must justify itself against the rebase cost.**

Godot's PR backlog (~5K) makes upstream merge unrealistic on any useful timeline. Maintaining a deep fork creates permanent rebase tax. The goal is to ship as much as possible through GDExtension + CompositorEffect + RenderingDevice, and only patch core Godot where Phase 0 proves it's truly unavoidable.

### Phase 0 Determines the Boundary

Meridian's Phase 0 answers the critical question: can CompositorEffect + RenderingDevice own enough of the pipeline for a visibility buffer renderer? If yes, everything stays GDExtension. If no, the engine patches should be as small and contained as possible (expose specific hooks, not rewrite subsystems).

### Per-Category Delivery

| Category | Target Delivery | Fallback if Needed | Rebase Risk |
|---|---|---|---|
| Rendering | CompositorEffect + GDExtension | Minimal engine patches for render hooks | Low-Medium |
| Lighting | CompositorEffect + GDExtension (NVIDIA fork approach) | Engine patches for lighting integration | Medium |
| Physics Sim | GDExtension (compute via RenderingDevice) | — | None |
| VFX / Particles | GDExtension | Engine patches for particle pipeline hooks | Low |
| World Streaming | GDExtension + scene tree extensions | Engine patches for streaming hooks | Low-Medium |
| Animation | GDExtension | — | None |
| Procedural Gen | GDExtension | — | None |
| Audio | GDExtension | — | None |

### If Engine Patches Are Needed

Maintain a fork repo that tracks upstream Godot. Keep the diff minimal and contained. Rebase on each Godot release. The smaller the diff, the lower the maintenance cost. A 500-line patch is rebeasable. A 50,000-line rewrite is not.

## Godot Official Roadmap Alignment

| Feature | Godot Official Plans | Our Approach |
|---|---|---|
| Dense geometry | GPU-driven meshlet auto-LOD | Full Nanite-class virtualized renderer (Meridian) |
| Lighting | Full path tracing (Juan's stated direction) | Path tracing + hybrid fallback (Aurora) |
| Cloth/Fluid | No official plans in core | GPU compute cloth and fluid (Cascade) |
| VFX | GPUParticles3D improvements | Full Niagara-class system (future) |
| World streaming | No announced plans | World partition equivalent (future) |
| Animation | AnimationTree improvements | Control Rig equivalent (future) |

Our work is complementary to Godot's official direction, not conflicting. Where Godot plans incremental improvements, we build the full competitive system. Where Godot has no plans, we fill the gap.
