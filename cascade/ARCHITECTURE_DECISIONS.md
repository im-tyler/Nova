# Physics Sim: Architecture Decisions

Last updated: 2026-03-24

## ADR-001: PhysX 5.6 as algorithmic reference, not runtime dependency

Status:

- accepted

Decision:

- study PhysX 5.6's open-source GPU solver code (BSD-3) for algorithms and data structures
- do not integrate PhysX as a runtime dependency

Reason:

- PhysX GPU requires CUDA (NVIDIA-only), which contradicts the no-vendor-lock-in requirement
- porting 500+ CUDA kernels to Vulkan is enormous work with uncertain payoff
- the algorithms (XPBD, FEM, SPH, spatial hashing) are portable math — the value is in understanding them, not in running the CUDA code
- BSD-3 license allows studying and reimplementing freely

Implication:

- all solver code is written from scratch targeting portable GPU compute
- PhysX source is the textbook for solver architecture, optimization patterns, and data layout
- Blast SDK (also BSD-3, physics/graphics agnostic) may be integrable more directly for destruction since its core is CPU and API-agnostic

## ADR-002: No vendor lock-in — solvers must run on any Vulkan/WebGPU GPU

Status:

- accepted

Decision:

- all GPU compute must work on NVIDIA, AMD, Intel, and Apple GPUs
- no CUDA, no ROCm, no Metal-only paths as primary targets
- Vulkan compute is the primary target; WebGPU, Metal, OpenGL are secondary

Reason:

- Godot's value proposition is accessibility and openness
- a Godot physics feature that only works on NVIDIA hardware is not a real Godot feature
- Vulkan runs on NVIDIA, AMD, Intel (Linux/Windows); WebGPU adds browser + macOS reach

Implication:

- CUDA translation layers (ZLUDA, HIPIFY) are not acceptable as the primary path
- the solver is written in GLSL compute shaders (compilable to SPIR-V) or WGSL

## ADR-003: GPU compute is mandatory for the solver

Status:

- accepted

Decision:

- cloth, fluid, and destruction solvers run on GPU via compute shaders

Reason:

- CPU cloth/fluid cannot compete with Unreal's GPU implementations
- GPU parallelism is fundamental to competitive constraint and particle solving
- Godot's RenderingDevice exposes Vulkan compute capability

Implication:

- a CPU fallback may exist for the Compatibility backend but is not the primary target
- requires Vulkan-capable hardware for full performance

## ADR-004: Cloth first, fluid second, destruction third

Status:

- accepted

Decision:

- build cloth simulation first, then fluid, then destruction

Reason:

- cloth is most commonly needed in game production
- XPBD cloth is well-understood with good GPU parallelization
- fluid requires additional rendering work (surface reconstruction)
- destruction is best served by studying Blast (already physics-agnostic) and can use a more direct integration path

## ADR-005: XPBD for cloth, SPH for fluid, Blast-style for destruction

Status:

- accepted

Decision:

- use Extended Position-Based Dynamics for cloth (industry standard)
- use Smoothed Particle Hydrodynamics for fluid (initial solver)
- use Voronoi fracture with rigid debris for destruction (Blast model)

Reason:

- XPBD: same algorithm as Chaos Cloth, well-studied GPU parallelization, stable at game time steps
- SPH: naturally parallelizable on GPU, good for small-to-medium game effects
- Blast-style fracture: proven design, Blast SDK itself is physics/graphics agnostic and BSD-3

## ADR-006: Separate from Godot's rigid body physics engine

Status:

- accepted

Decision:

- GPU solvers are independent from Godot Physics / Jolt
- solvers read collision geometry from the physics world but solve independently on GPU

Reason:

- GPU compute solver cannot share a pipeline with CPU physics engines
- decoupling prevents dependency on any specific physics backend
- users keep their choice of rigid body engine

Implication:

- collision geometry extraction from physics world to GPU buffers is needed
- two-way coupling (fluid pushing rigid bodies) is deferred

## ADR-007: Delivery starts as GDExtension, escalates if needed

Status:

- accepted

Decision:

- attempt GDExtension delivery using RenderingDevice compute (C++) or wgpu (Rust)
- escalate to engine module only if GDExtension compute access proves insufficient

Reason:

- GDExtension is easier for users to adopt
- Rapier proves Rust GDExtension works for physics in Godot
- RenderingDevice compute should be sufficient for solver dispatch

Implication:

- rendering integration (getting sim output into Godot's render pipeline) is the likely bottleneck
- if vertex buffer sharing between compute and rendering is blocked by GDExtension, engine module is the fallback

## ADR-008: Primary implementation language and GPU abstraction

Status:

- accepted

Decision:

- C++ with Godot RenderingDevice (Vulkan compute) as the primary implementation
- GLSL compute shaders as the shader language
- portable shaders compilable to SPIR-V, cross-compilable to WGSL/MSL via SPIRV-Cross or Naga when platform expansion is needed

Reason:

- fastest path to a working Godot feature — no external GPU abstraction layer needed
- C++ aligns with PhysX 5.6 reference code (easier to study and reimplement)
- RenderingDevice is Godot's native compute API, deepest integration
- avoids the two-GPU-context problem that wgpu would introduce
- GLSL compute shaders compile to SPIR-V for Vulkan, and SPIRV-Cross can later emit WGSL (WebGPU), GLSL 4.3 (OpenGL), MSL (Metal)
- platform expansion is a later concern — ship on Vulkan first, port shaders when needed

Implication:

- WebGPU and Metal support deferred to Phase 4 (platform expansion)
- shader code must be written as clean, portable GLSL with no Vulkan-specific extensions where avoidable
- Rust + wgpu remains a future option if the two-GPU-context problem is solved or if Godot gains a wgpu backend

## ADR-009: Blast SDK integration vs custom destruction

Status:

- accepted

Decision:

- integrate Blast SDK directly for destruction and fracture

Reason:

- Blast is BSD-3, deliberately physics-agnostic and graphics-agnostic
- it handles the hardest parts: Voronoi fracture generation, damage propagation, support graphs, multi-layer destruction
- Blast is C++ with a clean layered API (NvBlast low-level, NvBlastTk toolkit, extensions)
- the PhysX extension for Blast manages PxActors and PxJoints, but since we use Jolt, we use Blast's agnostic core and write our own Jolt bridge
- reimplementing fracture generation and damage models would take months with no quality advantage

Implication:

- Blast SDK is vendored as a dependency (BSD-3, same license compatibility as Godot)
- a thin bridge layer maps Blast's fracture output to Jolt rigid bodies for debris physics
- Blast's fracture authoring is offline (pre-fracture in editor); runtime triggers damage and separation
- rendering of fractured pieces uses standard Godot MeshInstance3D or Meridian VGeoMeshInstance3D for dense debris

## ADR-010: Fluid solver algorithm

Status:

- accepted

Decision:

- SPH (Smoothed Particle Hydrodynamics) as the initial fluid solver
- FLIP/APIC as a later upgrade for large-scale liquid behavior

Reason:

- SPH is simpler to implement in GPU compute, naturally parallelizable
- good for small-to-medium game effects (splashes, blood, lava, potion physics)
- PhysX 5.6's PBD particle system provides SPH reference implementation
- FLIP/APIC is better for large-scale liquid behavior but significantly more complex (requires grid-particle transfer)
- shipping SPH first lets us validate the full pipeline (solver → surface reconstruction → rendering) before adding solver complexity

Implication:

- Phase 3 (fluid) uses SPH
- Phase 6 (advanced) adds FLIP/APIC as an upgrade option for large-scale fluid
- surface reconstruction is solver-agnostic (works with both SPH and FLIP particle output)
