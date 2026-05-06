# Lighting: Architecture Decisions

Last updated: 2026-03-24

## ADR-001: Build on the NVIDIA RTX Godot fork

Status:

- accepted

Decision:

- use the NVIDIA RTX path-tracing fork as the foundation rather than building a path tracer from scratch

Reason:

- the path tracer is MIT-licensed, GPU-agnostic in core design, and intended for upstream merge
- building a production path tracer from scratch would take 12+ months of pure rendering work

Implication:

- track NVIDIA fork updates and contribute upstream where possible
- accept dependency on NVIDIA's architectural choices for the path tracing core

## ADR-002: Vendor-agnostic denoiser is a hard requirement

Status:

- accepted

Decision:

- ship with at least one denoiser that works on AMD and Intel GPUs, not just NVIDIA

Reason:

- DLSS Ray Reconstruction is NVIDIA-only
- a Godot feature that only works well on one vendor's hardware is not a real Godot feature

Implication:

- evaluate OIDN, AMD FidelityFX Denoiser, and custom temporal approaches
- may need to support multiple denoisers with a selection mechanism

## ADR-003: Hybrid fallback is part of the architecture

Status:

- accepted

Decision:

- design a tiered quality system: path tracing > probe-based GI > screen-space effects

Reason:

- Lumen's competitive advantage is hardware reach, not raw quality
- pure path tracing abandons the majority of current Godot users
- Godot's value proposition includes accessibility

Implication:

- the lighting data format must work for both path-traced and probe-based paths
- the fallback path is a real engineering effort, not an afterthought

## ADR-004: Meridian integration is a first-class goal

Status:

- accepted

Decision:

- design the lighting system to work with Meridian's virtualized geometry from the start

Reason:

- dense geometry without good lighting defeats the purpose of both projects
- BVH construction from cluster data is more efficient than from raw meshes
- shared material format reduces duplication

Implication:

- coordinate geometry representation between Meridian and Aurora
- shared streaming infrastructure where possible

## ADR-005: Delivery vehicle follows Meridian

Status:

- accepted

Decision:

- if Meridian requires an engine module or fork, Aurora follows the same path

Reason:

- lighting integration requires deep renderer access (same as geometry)
- maintaining two separate delivery strategies for tightly coupled systems is wasteful

## ADR-006: Primary vendor-agnostic denoiser

Status:

- accepted

Decision:

- Intel OIDN (Open Image Denoise)

Reason:

- Apache 2.0 license, fully open
- runs on AMD, Intel, and NVIDIA GPUs (no vendor lock-in)
- supports Vulkan buffer sharing via external memory (zero-copy interop possible with Godot's RenderingDevice)
- OIDN 3 (scheduled H2 2026) adds temporal denoising, critical for real-time path tracing flicker reduction
- Academy Award-winning quality (Technical Achievement Award 2025)
- fast quality mode provides 1.5-2x speedup for interactive/real-time use
- denoising at half resolution with upscaling is a viable optimization path

Implication:

- DLSS Ray Reconstruction remains supported as a premium NVIDIA-only option
- OIDN is the default denoiser for cross-vendor support
- must benchmark OIDN fast mode at game frame rates (target: under 4ms at 1080p) during Phase 0

## ADR-007: Lighthugger as rendering reference for Meridian-Aurora geometry sharing

Status:

- accepted

Decision:

- Aurora's BVH construction should consume Meridian's cluster hierarchy data rather than building BVH from raw meshes

Reason:

- Meridian already builds a clustered LOD hierarchy with per-cluster bounds
- this data maps directly to BVH construction inputs
- avoids duplicate geometry processing
- Lighthugger (MIT, Vulkan meshlet + visibility buffer renderer) proves this architecture works

Implication:

- Aurora Phase 2 (Meridian integration) depends on a shared geometry data format
- define the cluster-to-BVH interface during Phase 0
