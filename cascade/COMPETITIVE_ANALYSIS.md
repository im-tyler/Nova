# Physics Sim: Competitive Analysis

Last updated: 2026-03-24

## Unreal Engine

### Chaos Cloth

- GPU-accelerated XPBD solver
- deep integration with skeletal meshes
- vertex painting for constraint weights and masks
- self-collision
- backstop constraints (prevent cloth from going inside body)
- wind and force field support
- LOD system for cloth simulation
- per-bone collision capsules
- production-proven across major titles

### Niagara Fluids

- GPU particle system with fluid simulation modules
- FLIP/APIC solvers for liquid behavior
- SPH modules for viscous fluids
- gas/smoke simulation via grid-based solvers
- surface reconstruction and rendering
- deep integration with Niagara VFX system
- material interaction (wet surfaces, foam, spray)
- scalable from small effects to medium-scale environments

### Chaos Destruction (related)

- GPU-accelerated fracture and destruction
- voronoi-based pre-fracture
- runtime fracture
- debris and dust particle generation
- not in scope for Cascade v1 but related technology

## Godot Current State

### SoftBody3D

- CPU-based soft body deformation
- generic soft body, not cloth-specific
- poor performance with constant collisions
- no vertex painting for constraints
- no GPU acceleration
- usable for jelly-like deformation, not garments

### Community: Silkload

- bone-driven cloth simulation
- verlet integration
- CPU-only
- functional for simple cloaks/capes
- limited collision handling
- no self-collision

### Community: Godot Rapier Physics

- Rust-based physics engine replacement
- fluid simulation via Salva library (SPH)
- SIMD parallelism, no GPU compute
- 2D and 3D support
- best available fluid option in Godot ecosystem
- deterministic mode available
- CPU-bound performance ceiling

### Community: Various Verlet Addons

- simple verlet-based cloth
- mostly 2D or basic 3D
- no GPU acceleration
- educational/prototype quality

## Gap Summary

### Cloth

| Feature | Unreal Chaos Cloth | Godot Best Available |
|---|---|---|
| Solver | GPU XPBD | CPU verlet (addon) |
| Skeletal mesh integration | Deep | Basic (bone-driven) |
| Vertex painting | Yes | No |
| Self-collision | Yes | No |
| Backstop | Yes | No |
| LOD | Yes | No |
| Wind/forces | Yes | Limited |
| Production quality | Proven | Prototype |

### Fluid

| Feature | Unreal Niagara Fluids | Godot Best Available |
|---|---|---|
| Solver | GPU FLIP/APIC/SPH | CPU SPH (Rapier/Salva) |
| Surface reconstruction | Yes | Basic |
| Rendering integration | Deep (Niagara) | Limited |
| Material interaction | Yes (wet, foam) | No |
| Scale | Medium environments | Small effects |
| Production quality | Proven | Functional |

## Strategic Assessment

The cloth gap is more important to close first because:

1. cloth is needed by more game projects (character garments, flags, curtains)
2. the algorithmic path is clearer (XPBD is well-documented)
3. the integration story is more defined (skeletal mesh binding)
4. fluid requires additional rendering work (surface reconstruction) that is harder in Godot's current pipeline

Fluid is valuable but more niche for typical game production. Prioritize cloth.
