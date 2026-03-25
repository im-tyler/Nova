# Lighting: Implementation Backlog

Last updated: 2026-03-24

Prioritized for production path tracing first, then hybrid fallback.

## P0: Assessment and Baseline

- build and evaluate NVIDIA RTX Godot fork
- inventory renderer changes
- denoiser survey and recommendation
- performance baseline on multi-vendor hardware
- Meridian integration feasibility

## P1: Production Path Tracing

- clean integration with Godot material system
- vendor-agnostic denoiser integration
- validate all Godot light node types
- quality settings and presets
- editor preview integration
- performance profiling and optimization
- stability and edge case handling

## P2: Meridian Integration

- BVH construction from cluster/page data
- ray intersection against virtualized geometry
- shared material resolve
- shadow integration between systems
- streaming-aware BVH updates

## P3: Hybrid Fallback

- irradiance probe grid system
- reflection probe system
- screen-space GI fallback
- quality tier selection logic
- hardware detection and auto-configuration
- graceful degradation under performance pressure

## P4: Advanced Lighting Features

- volumetric lighting and fog
- virtual shadow maps
- decal lighting integration
- dynamic geometry response optimization
- emissive geometry improvements

## P5: Large World and Streaming

- lighting data streaming for large worlds
- probe grid streaming
- BVH streaming aligned with geometry streaming
- world partition integration
