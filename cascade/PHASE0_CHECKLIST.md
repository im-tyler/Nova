# Physics Sim: Phase 0 Checklist

Last updated: 2026-03-24

Phase 0 validates GPU compute physics in Godot and establishes the solver and platform strategy.

## Deliverables

- GPU XPBD cloth prototype (GLSL compute, Vulkan)
- performance comparison vs CPU baselines
- Godot RenderingDevice compute feasibility report
- PhysX 5.6 solver architecture study notes
- Blast SDK architecture assessment
- platform strategy recommendation
- integration point mapping

## Checklist

### PhysX 5.6 Study

- [ ] clone PhysX 5.6 source
- [ ] study PxDeformableSurface (cloth) solver architecture
- [ ] study particle system (fluid) solver architecture
- [ ] study spatial hashing implementation
- [ ] study GPU buffer layout and memory management patterns
- [ ] document key algorithms and data structures for reimplementation
- [ ] note optimization patterns relevant to Vulkan compute

### Blast SDK Study

- [ ] clone Blast SDK source
- [ ] study fracture generation (Voronoi)
- [ ] study damage model and support graph
- [ ] assess whether Blast can be integrated directly (it's physics/graphics agnostic)
- [ ] document if reimplementation is needed or if direct use works

### GPU XPBD Prototype

- [ ] implement basic XPBD solver as GLSL compute shaders
- [ ] distance constraints
- [ ] bending constraints
- [ ] attachment constraints (pinned vertices)
- [ ] simple collision with plane/sphere
- [ ] dispatch via Godot RenderingDevice
- [ ] validate correctness against reference CPU implementation
- [ ] measure performance (vertex count vs frame time)

### Godot Compute Feasibility

- [ ] test RenderingDevice compute dispatch from GDExtension (C++)
- [ ] test RenderingDevice compute dispatch from gdext (Rust) if applicable
- [ ] measure compute dispatch overhead
- [ ] test mesh vertex buffer update from compute output
- [ ] test sharing buffers between compute and rendering passes
- [ ] identify GDExtension limitations for this workload
- [ ] determine if engine module is needed

### Platform Assessment

- [ ] test GLSL compute shaders via SPIRV-Cross → WGSL feasibility
- [ ] assess Godot WebGPU backend timeline and status
- [ ] evaluate wgpu two-GPU-context overhead if Rust path is considered
- [ ] make platform strategy recommendation

### Integration Points

- [ ] map skeletal mesh data access from GDExtension
- [ ] map physics body collision geometry extraction
- [ ] map MeshInstance3D vertex buffer update path from compute
- [ ] identify vertex painting tool requirements
- [ ] assess editor preview feasibility

### Benchmarks

- [ ] define test cloth scenarios (flag, cape, curtain, skirt on character)
- [ ] measure SoftBody3D baseline performance
- [ ] measure Jolt soft body baseline
- [ ] compare GPU prototype against CPU baselines
- [ ] define target performance metrics

### Blast SDK Assessment

- [ ] clone Blast SDK and build
- [ ] study NvBlast low-level API
- [ ] study NvBlastTk toolkit layer
- [ ] prototype Voronoi fracture of a simple mesh
- [ ] design Jolt rigid body bridge for debris
- [ ] assess editor integration for pre-fracture authoring

## Phase 0 Exit Gate

Phase 0 is complete only when:

1. GPU XPBD cloth runs in Godot via compute shaders and is significantly faster than CPU
2. solver algorithms are understood from PhysX reference
3. Blast integration strategy is decided
4. platform strategy is recommended
5. GDExtension vs engine module decision is informed
