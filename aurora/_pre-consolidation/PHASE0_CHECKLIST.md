# Lighting: Phase 0 Checklist

Last updated: 2026-03-24

Phase 0 exists to understand what the NVIDIA fork gives us and what we need to build on top.

## Deliverables

- NVIDIA fork build and evaluation report
- renderer change inventory
- denoiser recommendation
- Meridian integration feasibility memo
- performance baseline on target hardware

## Checklist

### NVIDIA Fork Assessment

- [ ] clone and build the NVIDIA RTX Godot fork
- [ ] inventory all renderer changes vs stock Godot
- [ ] document which Godot material types are supported
- [ ] document which light types are supported
- [ ] test with standard Godot demo scenes
- [ ] identify missing features vs production needs (volumetrics, decals, etc.)
- [ ] evaluate ReSTIR DI and ReSTIR GI quality and performance

### Denoiser Evaluation

- [ ] test DLSS Ray Reconstruction quality (NVIDIA hardware)
- [ ] evaluate Intel OIDN as vendor-agnostic alternative
- [ ] evaluate AMD FidelityFX Denoiser
- [ ] compare quality and performance across denoisers
- [ ] make denoiser recommendation
- [ ] benchmark OIDN fast mode at 1080p (target: under 4ms)
- [ ] benchmark OIDN fast mode at 1440p
- [ ] test OIDN Vulkan buffer sharing with Godot's RenderingDevice
- [ ] evaluate OIDN 3 preview builds if available (temporal denoising)
- [ ] compare OIDN quality vs DLSS Ray Reconstruction at equivalent frame budgets

### Performance Baseline

- [ ] define benchmark hardware profiles (match Meridian profiles)
- [ ] measure path tracing performance on NVIDIA hardware
- [ ] measure path tracing performance on AMD hardware
- [ ] compare against Godot SDFGI baseline
- [ ] identify primary performance bottlenecks

### Meridian Geometry Sharing

- [ ] define cluster-to-BVH data format interface
- [ ] assess whether Meridian's cluster bounds can serve as BVH leaf nodes
- [ ] prototype BVH construction from cluster data

### Meridian Integration

- [ ] assess BVH construction path from cluster data
- [ ] assess material data sharing between visibility buffer and path tracer
- [ ] assess shadow integration strategy
- [ ] document integration plan

## Phase 0 Exit Gate

Phase 0 is complete only when:

1. the NVIDIA fork's capabilities and limitations are documented
2. the denoiser strategy is chosen
3. the Meridian integration path is sketched
4. performance baseline exists on at least two GPU vendors
