# Godot-Unreal Parity: Deep Synthesis and Concrete Plans

Last updated: 2026-03-24

## Critical Discovery: CompositorEffect Cannot Replace the Opaque Pass

This is the single most important finding from the research.

Godot's CompositorEffect documentation states: **"The general ideal is that the opaque pass as it happens now needs to be left as is."**

CompositorEffect can run code:

- BEFORE the opaque pass (after depth prepass)
- AFTER the opaque pass (before sky)
- BEFORE/AFTER transparent pass

But it **cannot replace or skip the opaque pass itself.** This means:

- Meridian cannot fully own the opaque render pipeline through CompositorEffect alone
- Aurora cannot fully replace Godot's lighting model through CompositorEffect alone

### What This Means for Delivery Strategy

**Approach A: Dual-render** — Render dense geometry via CompositorEffect as additional geometry, use depth buffer to prevent overdraw with Godot's standard pass. Dense meshes render through Meridian; standard meshes render through Forward+. They coexist.

Pros: no engine patches. Cons: two render paths, potential depth fighting, shadow integration is complex, material systems diverge.

**Approach B: Minimal engine patch** — Patch Godot to add a "skip opaque for this viewport" flag or add a custom render pass hook that can replace the opaque pass. Small, contained patch.

Pros: clean ownership of the render pipeline. Cons: rebase cost, but the patch is small.

**Approach C: Full engine module** — Build a custom rendering backend or significantly modify the Forward+ renderer.

Pros: full control. Cons: heavy rebase cost, essentially a fork.

**Recommendation: Start with A, prepare for B.** The dual-render approach works for v1 where only tagged dense meshes use Meridian and everything else uses standard Forward+. If/when full pipeline ownership is needed, a minimal patch (Approach B) keeps the diff small. Approach C is only needed if B proves insufficient.

This decision cascades to Aurora as well. Path tracing can work as a CompositorEffect that replaces the lighting resolve for the entire frame (the NVIDIA fork approach), but shadow and material integration get complicated without deeper access.

---

## Project-by-Project Concrete Plans

---

### 1. Meridian (Rendering) — Concrete Plan

**Foundation available:**

- meshoptimizer (vendored, BSD) — cluster building, meshlet generation, simplification
- Lighthugger (MIT) — Vulkan meshlet + visibility buffer renderer in C++20/GLSL, proof of the exact architecture
- Vulcanite — academic Nanite-style Vulkan implementation

**Concrete technical path:**

Phase 0 prototype should implement: meshlet culling → visibility buffer write → material resolve, using Lighthugger as architectural reference and meshoptimizer for the offline pipeline. Deploy via CompositorEffect running before the opaque pass, writing to depth to prevent standard geometry overdraw.

```
Offline: source mesh → meshoptimizer → cluster hierarchy → page format → .vgeo resource
Runtime: load pages → compute cull (frustum+occlusion) → rasterize to vis buffer → resolve materials → write depth
```

**Key metrics to hit:**

- 10M+ triangles at 60fps on mid-range GPU (RTX 3060 / RX 6700)
- memory bounded under camera traversal (< 2GB geometry budget)
- no visible cracks at LOD transitions

**Open risk:** CompositorEffect depth integration. Can Meridian write depth that Godot's standard pass respects? Phase 0 must answer this.

---

### 2. Aurora (Lighting) — Concrete Plan

**Foundation available:**

- NVIDIA RTX Godot fork (MIT) — full path tracer with ReSTIR DI/GI, Vulkan
- Intel OIDN (Apache 2.0) — vendor-agnostic denoiser, GPU-accelerated, supports Vulkan buffer sharing
- OIDN 3 (H2 2026) — adds temporal denoising for real-time path tracing flicker reduction

**Concrete technical path:**

Phase 0: clone NVIDIA fork, build, test, inventory changes. Evaluate OIDN as denoiser replacement for DLSS Ray Reconstruction.

The NVIDIA fork already modifies Godot's renderer at a deep level. Aurora's path is to take NVIDIA's changes and:

1. Replace DLSS RR with OIDN (vendor-agnostic)
2. Add hybrid fallback for non-RT hardware
3. Integrate with Meridian's geometry for BVH construction

**Denoiser decision: OIDN.** Close the open ADR. Reasons:

- Apache 2.0
- runs on AMD, Intel, NVIDIA GPUs
- Vulkan buffer sharing (zero-copy interop possible)
- OIDN 3 adds temporal denoising (critical for real-time)
- Academy Award-winning quality
- fast quality mode for interactive use

**Key risk:** OIDN's real-time performance at game frame rates. Path tracing + denoising must fit in 16ms. OIDN's fast mode claims 1.5-2x speedup — need to benchmark. If too slow, temporal accumulation with spatial filtering (custom) is the fallback.

**Open risk:** NVIDIA fork tracks Godot dev branch. If it diverges significantly, maintaining compatibility is work. Monitor NVIDIA's upstream merge progress.

---

### 3. Cascade (Physics) — Concrete Plan

**Foundation available:**

- PhysX 5.6 (BSD-3) — XPBD FEM cloth, SPH fluid, deformable body GPU solvers (CUDA, study only)
- Blast SDK (BSD-3) — destruction/fracture library, physics/graphics agnostic, C++
- Flow SDK (BSD-3) — sparse grid GPU gaseous fluid (smoke/fire), compute shaders
- WebGPU XPBD cloth references — multiple open implementations proving the algorithm works in compute shaders

**Concrete technical path:**

Phase 0: implement XPBD cloth as GLSL compute shaders dispatched via RenderingDevice. Validate on Godot.

```
Cloth: vertex positions → compute dispatch (XPBD constraints) → updated positions → write to mesh buffer
Fluid: particle positions → compute dispatch (SPH forces) → updated particles → surface reconstruction or pass to Tempest
Destruction: Blast SDK integration (CPU fracture generation) → rigid debris via Jolt → VFX via Tempest
```

**Blast integration decision:** Integrate Blast directly rather than reimplementing. It's:

- BSD-3
- deliberately physics/graphics agnostic (works with any physics engine)
- C++ with clean API
- handles the hard part (fracture generation, damage propagation, support graphs)
- debris rigid bodies feed into Jolt for physics, Tempest for VFX

**Key metric:** GPU cloth with 50K+ vertices at 60fps, competitive with Unreal Chaos Cloth on equivalent content.

**Open risk:** getting compute shader output back into Godot's mesh rendering pipeline efficiently from GDExtension. Phase 0 must validate the buffer update path.

---

### 4. Tempest (VFX) — Concrete Plan

**Foundation available:**

- PhysX Flow SDK (BSD-3) — sparse grid GPU volume simulation architecture (CUDA, study for reimplementation)
- Godot's GraphEdit control — existing node graph UI framework (used by VisualShader, AnimationTree)
- Dear ImGui node editor (imnodes) — C++ node graph if needed outside Godot editor

**No open-source Niagara equivalent exists.** This is original work.

**Concrete technical path:**

The particle system is three pieces: update (compute), sort (compute), render (graphics).

```
Update: per-particle compute shader runs module chain (forces, collision, spawn, death)
Sort: bitonic sort or radix sort for depth ordering (transparency)
Render: instanced billboards, mesh instances, or trail geometry
```

Module system design: each "module" is a GLSL compute shader snippet. The VFX graph editor (built on Godot's GraphEdit) generates the combined compute shader from the module chain. This is how Niagara works conceptually — the graph compiles to GPU code.

**Volume rendering for smoke/fire:** study Flow SDK's sparse grid architecture, reimplement in GLSL compute. Ray-march the density grid in a fragment shader.

**Key dependency:** Tempest's fluid rendering needs Cascade's solver output. Design the particle buffer format to be shared.

**Shared buffer format:**

```
struct Particle {
    vec3 position;
    vec3 velocity;
    float age;
    float lifetime;
    uint flags;
    // custom attributes follow
};
```

Cascade writes to this buffer (solver output). Tempest reads from it (rendering input). Same GPU buffer, no copy.

---

### 5. Atlas (World Streaming) — Concrete Plan

**Foundation available:**

- Open World Database (OWDB) — Godot addon, camera-based chunk streaming, multiplayer support
- Chunx — simpler Godot 4 streaming plugin
- Godot large world coordinates — already supported (double precision build)

**Concrete technical path:**

Study OWDB's architecture. It already solves basic chunking and streaming. Atlas extends it with:

1. Integration with Meridian's geometry page streaming (coordinate streaming decisions)
2. Data layer system (separate base geometry, foliage, gameplay objects into streamable layers)
3. Priority scheduling (velocity prediction, explicit hints)
4. Memory budget enforcement across all streaming systems

```
Atlas decides: "cells A, B, C are relevant"
  → Meridian: "load geometry pages for cells A, B, C"
  → Aurora: "load lighting data for cells A, B, C"
  → Cascade: "activate physics for cells A, B, C"
  → Tempest: "activate VFX for cells A, B, C"
```

**Key insight:** Godot already has ResourceLoader with async support and large world coordinates. Atlas is coordination and policy, not low-level I/O. This may not need engine patches at all.

**Key metric:** seamless streaming with no visible pop-in at 60fps for worlds > 10km².

---

### 6. Kinetic (Animation) — Concrete Plan

**Foundation available:**

- orangeduck/Motion-Matching (C++) — canonical implementation by Daniel Holden, inventor of Learned Motion Matching
- Open-Source-Motion-Matching-System — C++ rewrite of Unreal's motion matched animation sample
- SIGGRAPH Asia 2025 — "Environment-aware Motion Matching" with code
- Mesh2Motion — open source animation tool with control rigs

**Concrete technical path:**

Motion matching is the highest-value feature. The algorithm:

1. Build a motion database from mocap data (offline)
2. At runtime, search the database for the best matching pose + trajectory
3. Blend from current pose to matched pose

orangeduck's implementation is the reference. Port the search algorithm to GPU compute for many-character scenarios.

For procedural animation (IK, physics-driven):

- FABRIK IK solver (well-documented, simple to implement)
- Ragdoll blending (blend between animation and Jolt ragdoll based on hit reactions)
- Spring bone chains for secondary motion (hair, tails, accessories)

**Delivery:** GDExtension extending Skeleton3D and AnimationTree. Adds new node types (MotionMatchingNode, ProceduralIKNode, etc.).

---

### 7. Resonance (Audio) — Concrete Plan

**Foundation available:**

- LabSound (BSD-2) — C++ graph-based audio engine, WebAudio-derived, cross-platform, HRTF support
- Steam Audio (free SDK) — spatial audio with HRTF, occlusion, physics-based propagation. **Already has Godot GDExtension integrations.**

**Concrete technical path:**

Two-layer approach:

1. **Spatial audio: use Steam Audio.** It already works in Godot. Community GDExtensions exist. Don't reimplement HRTF, occlusion, or propagation.

2. **Programmable audio graph: build on LabSound or from scratch.** LabSound provides the DSP graph runtime (BSD-2). Wrap it in a Godot GDExtension. Build a visual graph editor using Godot's GraphEdit (same approach as Tempest's VFX graph).

```
Graph Editor (Godot GraphEdit) → generates audio graph description → LabSound runtime processes audio → output to Godot AudioServer
```

**Key insight:** Steam Audio for spatial audio means Resonance's unique value is the programmable graph, not the spatial engine. This significantly reduces scope.

---

### 8. Scatter (Procgen) — Concrete Plan

**Foundation available:**

- No open-source PCG framework for game engines
- Unreal 5.7 PCG docs and GDC talks (architecture reference)
- Godot's GraphEdit (graph editor UI)
- Godot's MultiMeshInstance3D (efficient instanced rendering for scattered objects)

**Concrete technical path:**

The PCG framework is a graph that transforms point sets:

```
Input (surface, volume, spline) → sample points → filter (slope, height, distance) → transform (random offset/rotation/scale) → output (MultiMesh instances or VGeoMeshInstance3D)
```

Each node in the graph is a point set operation. The graph editor uses Godot's GraphEdit. Execution happens in editor (for previewing) or at runtime (for dynamic worlds).

**Key insight:** Scatter is the simplest project. It's pure tooling — no GPU compute, no engine patches, no rendering changes. Point sampling, filtering, and instanced mesh placement. Could be built in GDScript/C++ as a pure EditorPlugin.

**Integration with Meridian:** scattered dense geometry instances use VGeoMeshInstance3D. Integration with Atlas: procedurally generated content respects streaming cell boundaries.

---

## Critical Gap Analysis

### GAP 1: CompositorEffect Cannot Replace Opaque Pass (SEVERITY: HIGH)

**Affects:** Meridian, Aurora

**The problem:** Meridian needs to render dense geometry through a visibility buffer. Aurora needs to replace the lighting model with path tracing. CompositorEffect only allows running code before/after the opaque pass, not replacing it.

**Mitigation:** Dual-render approach (Meridian renders alongside Forward+) works for v1 but has integration costs (shadow sharing, material system divergence, depth fighting). If this proves insufficient, a minimal engine patch (~100-500 lines) to add an opaque pass replacement hook is needed.

**Action:** Phase 0 MUST test dual-render feasibility. Specifically: can a CompositorEffect write to the depth buffer before the opaque pass, and will Godot's opaque pass respect that depth to skip occluded standard geometry?

### GAP 2: Getting Compute Shader Output Into Godot's Render Pipeline (SEVERITY: HIGH)

**Affects:** Cascade, Tempest

**The problem:** Cascade's cloth solver produces vertex positions in a GPU compute buffer. How do those get rendered? Options:

a) Read back to CPU, update MeshInstance3D — slow, defeats GPU purpose
b) Write directly to a vertex buffer that Godot's renderer reads — requires buffer sharing between compute and rendering
c) Render the cloth yourself via CompositorEffect — bypasses Godot's material system

**Mitigation:** Godot's RenderingDevice should allow creating a buffer used for both compute and vertex data. Phase 0 must validate this path. If it works, compute writes vertex data and Godot renders it normally. If not, custom rendering via CompositorEffect is the fallback.

**Action:** Phase 0 must prototype compute → vertex buffer → render path.

### GAP 3: OIDN Real-Time Performance at Game Frame Rates (SEVERITY: MEDIUM)

**Affects:** Aurora

**The problem:** Path tracing at 1080p+ plus OIDN denoising must fit within ~10ms (leaving 6ms for everything else at 60fps). OIDN's fast mode is designed for interactive use but "interactive" in offline rendering means seconds, not milliseconds.

**Mitigation:** OIDN 3 (H2 2026) adds temporal denoising which reduces per-frame work. Denoising at half resolution and upscaling is a common trick. If OIDN is too slow, a custom temporal accumulation + spatial filter (cheaper but lower quality) is the fallback.

**Action:** Benchmark OIDN fast mode at game-relevant resolutions and frame budgets during Aurora Phase 0.

### GAP 4: No Open-Source VFX/Particle Foundation (SEVERITY: MEDIUM)

**Affects:** Tempest

**The problem:** Every other Tier 1-2 project has an open-source foundation to build on. Tempest has none. The GPU particle system, module architecture, and VFX graph are all original work.

**Mitigation:** The individual pieces are well-understood (GPU particle update is a compute shader, sorting is standard, rendering is instanced). The hard part is the module/graph system — but Godot's existing GraphEdit + VisualShader architecture provides the UI framework, and the compilation model (graph → GLSL compute) mirrors what VisualShader already does for fragment shaders.

**Action:** Study Godot's VisualShader graph-to-GLSL compilation as the pattern for Tempest's VFX graph-to-compute compilation.

### GAP 5: Cross-Project Buffer Sharing (SEVERITY: MEDIUM)

**Affects:** Cascade → Tempest, Meridian → Aurora

**The problem:** Multiple projects need to share GPU buffer data:

- Cascade solver output → Tempest particle rendering
- Meridian cluster data → Aurora BVH construction
- Atlas streaming decisions → all other systems

If each project creates its own GPU resources independently, data copies between them kill performance.

**Mitigation:** Define a shared buffer registry as part of the common GPU infrastructure. All projects allocate through it. Buffers are tagged with usage flags (compute read/write, vertex, index, storage). This is the "shared GPU simulation layer" from the earlier architecture discussion.

**Action:** Design the shared buffer registry as part of Phase 0 infrastructure before individual projects create their own buffer management.

### GAP 6: Testing and Quality Assurance at Scale (SEVERITY: MEDIUM-LOW)

**Affects:** All projects

**The problem:** Solo founder building 8 interconnected GPU systems. How do you test cross-project integration? How do you catch regressions? GPU bugs are notoriously hard to debug.

**Mitigation:**

- Automated benchmark suite that runs all projects on reference scenes
- Per-project validation tools (already planned in Meridian's backlog)
- Integration test scenes that exercise cross-project paths (dense geometry + path tracing + cloth)
- Ship Tier 1 projects before starting Tier 2/3

**Action:** Define the benchmark suite and CI pipeline early. Use the same hardware profiles across all projects.

### GAP 7: Godot Version Compatibility (SEVERITY: LOW-MEDIUM)

**Affects:** All projects using RenderingDevice

**The problem:** Godot releases new versions. RenderingDevice API may change. CompositorEffect is marked experimental. GDExtension ABI may shift.

**Mitigation:** Target a specific Godot version (4.6 currently) and only update when the next LTS or major version is stable. GDExtension binary compatibility is maintained back to 4.1 per the gdext docs.

**Action:** Pin to Godot 4.6. Test against 4.7-dev periodically.

---

## Revised Execution Timeline

### Parallel Phase 0 (weeks 1-6)

Run these simultaneously:

- **Meridian Phase 0:** benchmark baseline, test CompositorEffect depth writing, prototype meshlet culling + vis buffer via CompositorEffect
- **Aurora Phase 0:** build NVIDIA fork, benchmark OIDN at game frame rates, inventory fork changes
- **Cascade Phase 0:** prototype XPBD cloth compute via RenderingDevice, validate compute → vertex buffer → render path
- **Shared:** design buffer registry, define benchmark hardware profiles

Phase 0 answers the three critical questions:

1. Can CompositorEffect host a visibility buffer renderer? (Meridian)
2. Is OIDN fast enough for real-time? (Aurora)
3. Can compute output feed Godot's render pipeline? (Cascade)

### Sequential Build (after Phase 0)

Based on Phase 0 results:

- **Meridian Phase 1-2:** offline pipeline + standalone renderer (12-20 weeks)
- **Aurora Phase 1:** production path tracing with OIDN (8-12 weeks, can overlap with Meridian Phase 2)
- **Cascade Phase 1:** GPU cloth node (10-14 weeks, independent of Meridian)
- **Tempest Phase 0-1:** GPU particles after Cascade Phase 1 delivers solver output
- **Atlas Phase 0-1:** world streaming after Meridian Phase 2 proves geometry streaming
- **Tier 3:** only after Tier 1-2 projects ship

---

## Foundation Reference Summary

| Project | Open Foundation | License | What You Get |
|---|---|---|---|
| Meridian | meshoptimizer | MIT/BSD | Cluster building, meshlet generation |
| Meridian | Lighthugger | MIT | Full vis buffer renderer reference |
| Aurora | NVIDIA RTX Godot fork | MIT | Production path tracer |
| Aurora | Intel OIDN | Apache 2.0 | Vendor-agnostic GPU denoiser |
| Cascade | PhysX 5.6 | BSD-3 | Solver algorithm reference |
| Cascade | Blast SDK | BSD-3 | Destruction/fracture (direct integration candidate) |
| Cascade | Flow SDK | BSD-3 | Volume fluid architecture reference |
| Tempest | Flow SDK | BSD-3 | Sparse grid volume sim reference |
| Tempest | Godot GraphEdit | MIT | Node editor UI framework |
| Atlas | OWDB | MIT | Godot streaming addon reference |
| Kinetic | orangeduck/Motion-Matching | — | Canonical motion matching reference |
| Resonance | LabSound | BSD-2 | Audio graph engine |
| Resonance | Steam Audio | Free SDK | Spatial audio (Godot ports exist) |
| Scatter | (none) | — | Original work, UE5.7 PCG as reference |
