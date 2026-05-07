# Project Aurora

Production-grade global illumination for Godot, building on the [NVIDIA RTX Godot fork](https://github.com/NVIDIA-RTX/godot) for path tracing and adding a hybrid fallback for non-RT hardware. Aurora's goal is Lumen-class results with broader hardware reach than pure path tracing alone.

**Status: research-only.** No implementation in this repository yet. The NVIDIA fork does the hardest work; aurora's job is integration, polish, and the fallback path. See [`_pre-consolidation/`](./_pre-consolidation/) for the full original planning docs (PROJECT_PLAN, ARCHITECTURE_DECISIONS, COMPETITIVE_ANALYSIS, IMPLEMENTATION_BACKLOG, PHASE0_CHECKLIST).

## Concept

Godot's current real-time GI is SDFGI plus screen-space reflections — functional but not Lumen-competitive. The NVIDIA RTX fork (March 2026, MIT-licensed) delivers full path tracing with ReSTIR DI/GI under Vulkan. That solves the hardest sub-problem.

Aurora's value is in:

1. Making the path tracer ship as a polished Godot feature, not a research fork.
2. Adding a vendor-agnostic denoiser so non-NVIDIA hardware is not abandoned.
3. Integrating with meridian's virtualized geometry — sharing BVH inputs from cluster bounds rather than rebuilding geometry.
4. Providing a hybrid fallback (probe + screen-space) so non-RT hardware still gets acceptable lighting.

## Status — Research Only

Nothing buildable yet. The NVIDIA fork is cloned into `aurora/nvidia-godot-rtx/` (gitignored — clone it yourself). Research notes for OIDN integration live in [`oidn-research/OIDN_NOTES.md`](./oidn-research/OIDN_NOTES.md).

## Plan

### Tier 1 — Production Path Tracing
Take the NVIDIA fork and make it production-ready in Godot: stable scene-tree integration, vendor-agnostic denoiser, performance profiling, compatibility with meridian's dense geometry.

### Tier 2 — Hybrid Fallback
Probe-based GI (irradiance + reflection probes) plus screen-space GI as the lower tier. Automatic quality scaling based on hardware capability. Shared lighting data format between path-traced and fallback paths.

### Tier 3 — Lumen Feature Parity
Real-time response to dynamic geometry/lighting, virtual shadow maps, volumetric lighting and fog, decal lighting, large-world streaming integration.

### Non-Goals for v1
Mobile/web, VR-specific optimization, custom denoiser research, replacing Godot's existing baked-lightmap workflow.

## Architecture Decisions

| ID | Decision | Reasoning |
|----|----------|-----------|
| ADR-001 | Build on the NVIDIA RTX Godot fork rather than from scratch | MIT-licensed, GPU-agnostic core, intended for upstream merge. Saves 12+ months of pure rendering work. |
| ADR-002 | Vendor-agnostic denoiser is a hard requirement | DLSS Ray Reconstruction is NVIDIA-only. A Godot feature must work on AMD and Intel. |
| ADR-003 | Hybrid fallback is part of the architecture, not an afterthought | Pure path tracing abandons most of Godot's current users. Tiered quality (path-traced > probe > screen-space) keeps the feature broadly useful. |
| ADR-004 | Meridian integration is a first-class goal | Dense geometry without good lighting defeats both projects. Cluster bounds map directly to BVH leaves. |
| ADR-005 | Delivery vehicle follows meridian | Lighting integration needs deep renderer access. If meridian becomes an engine module, aurora follows. |
| ADR-006 | Intel OIDN as the primary cross-vendor denoiser | Apache 2.0, runs on AMD/Intel/NVIDIA, Vulkan buffer-sharing via external memory, Academy Award-winning quality. OIDN 3 (H2 2026) adds temporal denoising. |
| ADR-007 | Aurora's BVH consumes meridian's cluster hierarchy | Cluster bounds map to BVH leaves; avoids duplicate geometry processing. Lighthugger (MIT, Vulkan meshlet + visibility buffer) proves this architecture works. |

## Competitive Analysis

| Feature | Unreal Lumen | Godot Current | NVIDIA Fork | Aurora Target |
|---|---|---|---|---|
| Diffuse GI | Multi-bounce | SDFGI (limited) | Path-traced | Path-traced |
| Specular reflections | Yes | SSR only | Path-traced | Path-traced |
| Dynamic response | Yes | Limited | Yes | Yes |
| Non-RT hardware | Yes (software RT) | SDFGI / SS effects | No | Hybrid fallback |
| RT hardware | Acceleration | No | Required | Yes |
| Dense geometry | Nanite | No | No | Via meridian |
| Volumetrics | Yes | Basic | Not yet | Later |
| Virtual shadows | Yes | No | No | Later |

Lumen's real competitive advantage is hardware reach, not raw quality. The NVIDIA fork solves the hardest problem (path tracing) but leaves the broadest one (hardware reach) unsolved. Aurora must address both.

## Phase 0 Checklist

### NVIDIA fork assessment
- [ ] Clone and build the fork.
- [ ] Inventory renderer changes vs stock Godot.
- [ ] Document supported material and light types.
- [ ] Test with standard Godot demo scenes.
- [ ] Identify missing features (volumetrics, decals, etc.).
- [ ] Evaluate ReSTIR DI/GI quality and performance.

### Denoiser evaluation
- [ ] Test DLSS Ray Reconstruction on NVIDIA hardware.
- [ ] Evaluate Intel OIDN as cross-vendor alternative.
- [ ] Evaluate AMD FidelityFX Denoiser.
- [ ] Benchmark OIDN fast mode at 1080p (target: under 4ms) and 1440p.
- [ ] Test OIDN Vulkan buffer sharing via external memory with Godot's RenderingDevice.
- [ ] Compare OIDN vs DLSS RR at equivalent frame budgets.

### Performance baseline
- [ ] Define benchmark hardware profiles (match meridian profiles).
- [ ] Measure path tracing on NVIDIA and AMD.
- [ ] Compare against Godot SDFGI baseline.
- [ ] Identify primary bottlenecks.

### Meridian integration
- [ ] Define cluster-to-BVH data format.
- [ ] Assess whether cluster bounds can serve as BVH leaves.
- [ ] Prototype BVH construction from cluster data.
- [ ] Document material data sharing between visibility buffer and path tracer.
- [ ] Document shadow integration strategy.

Exit gate: NVIDIA fork capabilities documented, denoiser strategy chosen, meridian integration sketched, performance baseline on at least two GPU vendors.

## Phase Plan

| Phase | Duration | Goal |
|-------|----------|------|
| Phase 0 | 3-5 weeks | Assessment and baseline (above). |
| Phase 1 | 8-12 weeks | Production path tracing — clean material integration, vendor-agnostic denoiser, light-node validation, editor preview. |
| Phase 2 | 6-10 weeks | Meridian integration — BVH from cluster data, ray intersection against virtualized geometry, shared material resolve. |
| Phase 3 | 8-12 weeks | Hybrid fallback — irradiance + reflection probes, screen-space GI, quality tier selection, hardware auto-detection. |
| Phase 4 | Selective | Volumetric lighting, virtual shadow maps, decal lighting, dynamic geometry, large-world streaming. |

## Primary Risks

1. **NVIDIA fork divergence** from Godot main makes maintenance expensive.
2. **Denoiser quality** — open-source denoisers may not match DLSS RR.
3. **Mid-range hardware performance** — path tracing is expensive; if the fallback lags, the feature feels incomplete.
4. **Material compatibility** — Godot's material system may not map cleanly to what the path tracer expects.
5. **Meridian dependency** — if meridian is delayed, the dense-geometry integration stalls.

## References

- NVIDIA RTX Godot fork: https://github.com/NVIDIA-RTX/godot
- NVIDIA RTXPT reference: https://github.com/NVIDIA-RTX/RTXPT
- NVIDIA RTXDI reference: https://github.com/NVIDIA-RTX/RTXDI
- Intel OIDN: https://www.openimagedenoise.org/ — https://github.com/RenderKit/oidn
- Lighthugger (Vulkan meshlet + visibility buffer renderer, reference for meridian sharing): https://github.com/expenses/lighthugger
- 80 Level coverage of NVIDIA fork: https://80.lv/articles/nvidia-launches-path-tracing-fork-of-godot-engine
- See [`oidn-research/OIDN_NOTES.md`](./oidn-research/OIDN_NOTES.md) for detailed OIDN integration notes (API, Vulkan buffer sharing, platform matrix).
