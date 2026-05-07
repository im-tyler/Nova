# Project Cascade

GPU physics for Godot 4.4+ — XPBD cloth, SPH fluid, Voronoi fracture — driven by compute shaders dispatched through Godot's `RenderingDevice`. Targets Chaos Cloth and Niagara Fluids in Unreal as the parity bar.

**Status: working prototype.** Five node types build on macOS (Apple Silicon) and run together in the showcase demo. Significant limitations remain (CPU readback every frame, no self-collision, no inter-solver coupling) — see `cascade/README.md` and the consolidated limitations list below.

## Layout

```
cascade/
  cascade/             -- the GDExtension (C++, godot-cpp + RenderingDevice)
    src/               -- node implementations + compute shader strings
    shaders/           -- standalone GLSL shader sources
    cascade.gdextension
    SConstruct
    README.md          -- build, install, node types, usage
  test-project/        -- Godot test project for the GDExtension
  blast-research/      -- NVIDIA Blast SDK integration notes (destruction roadmap)
  _pre-consolidation/  -- original planning docs (preserved)
```

The original planning docs (PROJECT_PLAN, ARCHITECTURE_DECISIONS, COMPETITIVE_ANALYSIS, COMPETITIVE_PLAN, IMPLEMENTATION_BACKLOG, PHASE0_CHECKLIST, RESEARCH_PAPERS) live under [`_pre-consolidation/`](./_pre-consolidation/). The Blast integration plan stays at its current home: [`blast-research/INTEGRATION_PLAN.md`](./blast-research/INTEGRATION_PLAN.md).

## Concept

Godot has no built-in cloth node and no fluid simulation. The closest options — `SoftBody3D`, the Silkload addon, and Rapier/Salva — are CPU-bound and don't approach Unreal's integration or performance. Cascade closes the GPU physics gap.

| Solution | Type | GPU | Quality |
|---|---|---|---|
| `SoftBody3D` | Built-in | No | Generic deformation only |
| Silkload | Addon | No | Bone-driven cloth, functional |
| Rapier/Salva | Addon | SIMD only | Best fluid option, CPU-bound |
| **Cascade** | **GDExtension** | **Yes (Vulkan compute)** | **Prototype quality** |

## Quick Status — What Builds and Runs Today

| Node | Base | Working |
|------|------|---------|
| `CascadeWorld` | Node3D | Coordinator. IMEX time-splitting (cloth 1/60s, fluid 1/120s, fracture event-driven). |
| `CascadeCloth` | MeshInstance3D | GPU XPBD cloth. Grid-generated or arbitrary source meshes. Constraint graph coloring for parallel solves. Sphere and plane colliders. |
| `CascadeFluid` | MultiMeshInstance3D | SPH fluid. GPU spatial hashing with bitonic sort + prefix-sum grid. Particles rendered as instanced spheres. |
| `CascadeFracture` | MeshInstance3D | Voronoi fracture, pre-fractured at setup. `apply_damage()` separates pieces as `RigidBody3D` with impulse. |
| `CascadeComputeTest` | MeshInstance3D | Diagnostic — validates the compute-to-mesh pipeline with a wave shader. |

See [`cascade/README.md`](./cascade/README.md) for build/install/usage.

## Plan

### Tier 1 — GPU Cloth
GPU XPBD solver, vertex painting for constraint weights, collision with physics bodies and self-collision, skeletal mesh integration, wind/force fields, LOD.

### Tier 2 — GPU Fluid
GPU SPH (then FLIP/APIC for large-scale), surface reconstruction (marching cubes / screen-space), basic viscosity and surface-tension controls, physics-body interaction.

### Tier 3 — Production Polish + Destruction
LOD for cloth, cloth/fluid interaction with meridian's dense geometry, deterministic mode for replays, broader material support, Blast-style fracture (see `blast-research/`).

### Non-Goals for v1
Ocean / large-scale water, hair simulation (related but distinct), mobile / web GPU compute.

## Architecture Decisions

| ID | Decision | Reasoning |
|----|----------|-----------|
| ADR-001 | PhysX 5.6 (BSD-3) as algorithmic reference, not runtime dependency | PhysX GPU requires CUDA. Algorithms are portable math; the value is in studying them, not running CUDA. |
| ADR-002 | No vendor lock-in — solvers run on any Vulkan/WebGPU GPU | Godot's value is openness. NVIDIA-only = not a real Godot feature. |
| ADR-003 | GPU compute is mandatory | CPU cloth/fluid cannot compete with Unreal. |
| ADR-004 | Cloth first, fluid second, destruction third | Cloth is most needed in production; XPBD is well-understood; destruction can use Blast directly. |
| ADR-005 | XPBD for cloth, SPH for fluid, Blast-style for destruction | Industry-standard, well-parallelized, proven. |
| ADR-006 | Separate from Godot's rigid-body physics engine | GPU compute can't share a pipeline with CPU physics; users keep their rigid-body engine choice (Jolt or otherwise). |
| ADR-007 | Delivery starts as GDExtension, escalates if needed | Easier adoption. Rapier proves Rust GDExtension works for physics; RenderingDevice compute should be sufficient for solver dispatch. |
| ADR-008 | C++ + Godot RenderingDevice (Vulkan compute) as the primary implementation | Fastest path to a working Godot feature. GLSL compute shaders are portable to other backends via SPIRV-Cross / Naga later. |
| ADR-009 | Integrate Blast SDK directly for destruction | BSD-3-style license, physics/graphics-agnostic. Reimplementing Voronoi fracture and damage models would take months with no quality advantage. |
| ADR-010 | SPH initial fluid solver, FLIP/APIC as later upgrade | SPH is simpler in compute, naturally parallel, good for small/medium effects. FLIP/APIC needed only for large-scale liquid. |

## Competitive Analysis

### Cloth
| Feature | Unreal Chaos Cloth | Godot Best Available | Cascade |
|---|---|---|---|
| Solver | GPU XPBD | CPU verlet (addon) | GPU XPBD |
| Skeletal mesh integration | Deep | Basic (bone-driven) | Not yet |
| Vertex painting | Yes | No | Not yet |
| Self-collision | Yes | No | Not yet |
| Backstop | Yes | No | Not yet |
| LOD | Yes | No | Not yet |
| Wind/forces | Yes | Limited | Yes (basic) |
| Production quality | Proven | Prototype | Prototype |

### Fluid
| Feature | Unreal Niagara Fluids | Godot Best Available | Cascade |
|---|---|---|---|
| Solver | GPU FLIP/APIC/SPH | CPU SPH (Rapier/Salva) | GPU SPH |
| Surface reconstruction | Yes | Basic | Not yet |
| Rendering integration | Deep (Niagara) | Limited | Not yet |
| Material interaction | Yes (wet, foam) | No | Not yet |
| Scale | Medium environments | Small effects | Small |
| Production quality | Proven | Functional | Prototype |

Cloth gap is more important to close first — broader production demand, clearer integration path. Fluid is more niche.

## Phase 0 Checklist

### PhysX 5.6 study
- [ ] Clone PhysX 5.6 source.
- [ ] Study `PxDeformableSurface` (cloth) solver architecture.
- [ ] Study particle system (fluid) solver architecture.
- [ ] Study spatial hashing implementation.
- [ ] Document key algorithms for reimplementation.
- [ ] Note optimization patterns relevant to Vulkan compute.

### Blast SDK study
See [`blast-research/INTEGRATION_PLAN.md`](./blast-research/INTEGRATION_PLAN.md) for the worked-through integration plan.

### GPU XPBD prototype
- [x] Implement basic XPBD solver as GLSL compute shaders.
- [x] Distance, bending, attachment constraints.
- [x] Simple plane and sphere collision.
- [x] Dispatch via Godot RenderingDevice.
- [ ] Validate correctness against reference CPU implementation.
- [ ] Measure performance (vertex count vs frame time) systematically.

### Godot compute feasibility
- [x] RenderingDevice compute dispatch from GDExtension (C++).
- [x] Mesh vertex buffer update from compute output.
- [ ] Eliminate CPU readback per frame (current bottleneck).
- [ ] Identify GDExtension limitations for this workload.

### Benchmarks
- [ ] Define test cloth scenarios (flag, cape, curtain, skirt on character).
- [ ] Measure `SoftBody3D` baseline.
- [ ] Measure Jolt soft body baseline.
- [ ] Compare GPU prototype vs CPU baselines.

## Current Limitations

- **CPU readback for mesh update** — every frame, vertex positions are read back GPU-to-CPU to update `ArrayMesh`/`MultiMesh`. Main performance bottleneck.
- **No self-collision.** Cloth does not detect or resolve self-intersection.
- **Basic Voronoi fracture.** No pre-scored patterns, no hierarchical fracture, no runtime re-fracture of pieces.
- **No inter-solver coupling.** Cloth and fluid don't interact with each other.
- **Limited collider types.** Cloth supports sphere and plane only — no mesh colliders.
- **No XPBD long-range attachment constraints.** Distance and bending only.
- **SPH boundary handling.** Position clamping at domain bounds, not proper boundary particles.

## References

### Physics engines
- PhysX SDK: https://github.com/NVIDIA-Omniverse/PhysX
- Blast SDK: https://github.com/NVIDIAGameWorks/Blast
- Newton physics: https://github.com/newton-physics/newton
- Rapier: https://rapier.rs/
- Jolt: https://github.com/jrouwe/JoltPhysics

### GPU compute
- wgpu: https://github.com/gfx-rs/wgpu
- Diligent Engine: https://github.com/DiligentGraphics/DiligentEngine

### Cloth
- WebGPU XPBD cloth: https://github.com/jspdown/cloth
- WebGPU cloth simulator: https://github.com/ccincotti3/webgpu_cloth_simulator
- OpenGL compute cloth: https://github.com/likangning93/GPU_cloth
- Bevy XPBD: https://joonaa.dev/blog/02/bevy-xpbd-0-1-0

### Research
The original `RESEARCH_PAPERS.md` digests 10 SIGGRAPH papers relevant to multi-physics coupling, IMEX integration, fluid-solid coupling, and combustion. See [`_pre-consolidation/RESEARCH_PAPERS.md`](./_pre-consolidation/RESEARCH_PAPERS.md).
