# Project Aurora

Last updated: 2026-03-24

## Mission

Build a production-quality global illumination and lighting system for Godot that closes the gap with Unreal's Lumen, leveraging NVIDIA's open-source RTX path-tracing fork as a foundation and extending it with hybrid fallbacks for broader hardware support.

## Context

### The Gap

Godot's current real-time GI is SDFGI (signed distance field global illumination) plus screen-space reflections and AO as post-processing. This is functional but does not compete with Lumen's quality, which provides:

- multi-bounce diffuse GI
- specular reflections
- software ray tracing fallback
- hardware RT acceleration
- real-time response to lighting and geometry changes

### NVIDIA RTX Godot Fork (March 2026)

NVIDIA released an MIT-licensed path-tracing fork of Godot at GDC 2026 with:

- full path tracing under Vulkan (GPU-agnostic core)
- ReSTIR DI (direct illumination importance sampling)
- ReSTIR GI (indirect lighting)
- DLSS Ray Reconstruction denoiser (NVIDIA-only, second denoiser planned)
- Shader Execution Reordering
- emissive triangle, environment map, and analytic light support
- intended for upstream merge into Godot

This is a massive head start. The path tracer itself is the hardest part of the system and it already exists.

### Godot's Official Direction

Juan Linietsky has stated Godot will pursue full path tracing rather than a Lumen-style hybrid, arguing it saves complexity for a small team. This aligns with the NVIDIA fork. The risk is that pure path tracing requires RT hardware, leaving non-RT GPUs behind.

## Product Goal

### Tier 1: Production Path Tracing

Take the NVIDIA fork's path tracing and make it production-ready within Godot:

- stable integration with Godot's scene tree, materials, and lighting nodes
- vendor-agnostic denoiser (not just DLSS Ray Reconstruction)
- performance profiling and optimization
- compatibility with Project Meridian's dense geometry

### Tier 2: Hybrid Fallback

Add a fallback GI path for hardware without ray tracing support:

- probe-based GI (irradiance probes, reflection probes)
- screen-space GI and reflections as a lower tier
- automatic quality scaling based on hardware capability
- shared lighting data format between path-traced and fallback paths

### Tier 3: Feature Parity with Lumen

- real-time response to dynamic geometry and light changes
- virtual shadow maps or equivalent for dense-geometry shadows
- volumetric lighting and fog
- decal lighting integration
- large-world streaming integration

## Non-Goals for v1

- mobile or web support
- VR-specific optimizations
- custom denoiser research (use existing open-source denoisers)
- replacing Godot's existing baked lightmap workflow

## Strategic Position

### What We Start With

The NVIDIA fork provides the core path tracer. This is not starting from zero.

### Where Our Value Is

1. Making the path tracer work as a polished, integrated Godot feature rather than a research fork
2. Adding fallbacks so non-RT hardware is not abandoned
3. Integrating with Meridian's dense geometry and visibility buffer
4. Production hardening: stability, performance, edge cases

### Relationship to Meridian

Meridian's visibility buffer and cluster hierarchy are natural inputs to a path tracer. Dense geometry that is already LOD-selected and streamed is exactly what a path tracer wants for BVH construction and ray intersection. The two projects should share:

- geometry representation awareness
- streaming infrastructure
- material data format for the constrained PBR subset

## Core Decisions

### 1. Build on the NVIDIA fork

Do not build a path tracer from scratch. The NVIDIA fork is MIT-licensed, GPU-agnostic in its core, and intended for upstream merge. Start there.

### 2. Vendor-agnostic denoiser is required

DLSS Ray Reconstruction is NVIDIA-only. A production system needs at least one open denoiser. Candidates:

- OIDN (Intel Open Image Denoise) — CPU and GPU, widely portable
- AMD FidelityFX Denoiser
- Custom temporal accumulation with spatial filtering

### 3. Hybrid fallback is strategically important

Pure path tracing cuts off a large portion of the Godot user base. A tiered approach where path tracing is the high-end mode and probe/screen-space GI is the fallback makes the feature broadly useful.

### 4. Delivery vehicle matches Meridian

If Meridian ends up as an engine module or fork, Aurora should follow the same delivery path. The lighting system needs deep renderer integration regardless.

## Phase Plan

### Phase 0: Assessment and Baseline

Duration: 3 to 5 weeks

Deliverables:

- clone and build NVIDIA RTX Godot fork
- inventory of changes NVIDIA made to Godot's renderer
- performance baseline on target hardware
- gap analysis: what the fork provides vs what production use requires
- denoiser survey and recommendation
- integration feasibility with Meridian

Exit criteria:

- clear understanding of what the NVIDIA fork does and does not provide
- denoiser decision
- integration strategy documented

### Phase 1: Production Path Tracing

Duration: 8 to 12 weeks

Build:

- integrate NVIDIA path tracer cleanly with Godot material system
- add vendor-agnostic denoiser
- validate against Godot lighting node types
- performance profiling and initial optimization
- basic editor integration (preview, quality settings)

Exit criteria:

- path tracing works reliably in Godot scenes with standard materials
- denoiser produces acceptable quality on non-NVIDIA hardware

### Phase 2: Meridian Integration

Duration: 6 to 10 weeks

Build:

- BVH construction from Meridian's cluster/page data
- ray intersection against virtualized geometry
- shared material resolve between visibility buffer and path tracer
- shadow integration between Meridian's shadow pass and Aurora's lighting

Exit criteria:

- dense geometry scenes render with path-traced lighting
- no duplicate geometry representation for raster vs RT

### Phase 3: Hybrid Fallback

Duration: 8 to 12 weeks

Build:

- irradiance probe grid system
- reflection probe system
- screen-space GI fallback
- quality tier selection (path traced > probe-based > screen-space)
- automatic hardware detection and tier assignment

Exit criteria:

- scenes look acceptable on non-RT hardware
- path tracing activates automatically on capable hardware

### Phase 4: Advanced Features

Build selectively:

- volumetric lighting and fog
- virtual shadow maps
- decal lighting
- dynamic geometry response
- large-world streaming integration

## Primary Risks

1. **NVIDIA fork divergence** — if NVIDIA's fork diverges from Godot's main branch, maintaining compatibility becomes expensive
2. **Denoiser quality** — open-source denoisers may not match DLSS RR quality, creating a perceived quality gap
3. **Performance on mid-range hardware** — path tracing is expensive; if the fallback is too far behind, the feature feels incomplete
4. **Material compatibility** — Godot's material system may not map cleanly to what the path tracer expects
5. **Meridian dependency** — if Meridian is delayed, Aurora's dense geometry integration stalls

## Related Projects

- [Project Meridian](/Users/tyler/Documents/renderer/PROJECT_PLAN.md) — dense geometry renderer
- [NVIDIA RTX Godot Fork](https://github.com/NVIDIA-RTX/godot) — foundation for path tracing

## Sources

- NVIDIA RTX Godot fork: https://github.com/NVIDIA-RTX/godot
- NVIDIA GDC 2026 announcements: https://www.nvidia.com/en-us/geforce/news/gdc-2026-nvidia-geforce-rtx-announcements/
- 80 Level coverage: https://80.lv/articles/nvidia-launches-path-tracing-fork-of-godot-engine
- GameFromScratch coverage: https://gamefromscratch.com/nvidia-release-rtx-powered-godot-fork/
- CG Channel coverage: https://www.cgchannel.com/2026/03/get-nvidias-new-path-tracing-fork-of-the-godot-game-engine/
- NVIDIA RTXPT reference: https://github.com/NVIDIA-RTX/RTXPT
- NVIDIA RTXDI reference: https://github.com/NVIDIA-RTX/RTXDI
- Intel OIDN: https://www.openimagedenoise.org/
- OIDN GitHub: https://github.com/RenderKit/oidn
- OIDN 3 temporal denoising: https://www.cgchannel.com/2026/01/open-image-denoise-3-will-support-temporal-denoising/
- Lighthugger: https://github.com/expenses/lighthugger
