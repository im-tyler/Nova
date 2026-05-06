# Lighting: Competitive Analysis

Last updated: 2026-03-24

## Unreal Lumen

### What It Is

Lumen is Unreal's real-time global illumination and reflections system. It provides:

- multi-bounce diffuse GI
- high-quality specular reflections
- real-time response to dynamic lighting and geometry changes
- software ray tracing fallback (no RT hardware required)
- hardware RT acceleration when available
- integration with Nanite's dense geometry

### How It Works

Lumen uses a hybrid approach:

1. **Surface cache** — screen-space and world-space radiance caching
2. **Software ray tracing** — traces against signed distance fields and mesh cards
3. **Hardware RT** — optional acceleration for higher quality
4. **Temporal accumulation** — builds up quality over frames

### Strengths

- works on non-RT hardware via software fallback
- deeply integrated with Unreal's renderer
- handles dynamic scenes well
- good quality/performance ratio on mid-range hardware

### Weaknesses

- complex implementation (Epic has 150+ graphics engineers)
- software RT quality is lower than hardware RT
- can produce light leaking in certain configurations
- large memory footprint for the surface cache

## Godot Current State (2026-03-24)

### SDFGI

- signed distance field based GI
- provides diffuse GI and some specular
- limited to static/semi-static scenes
- quality gap vs Lumen is significant

### Screen-Space Effects

- SSAO, SSR, SSIL
- cheap but limited to screen-visible information
- no multi-bounce, no off-screen contribution

### Baked Lightmaps

- LightmapGI node
- high quality for static lighting
- no real-time response to changes
- long bake times for large scenes

### NVIDIA RTX Fork (new)

- full path tracing
- ReSTIR DI + ReSTIR GI
- highest possible quality
- requires RT hardware (currently)
- NVIDIA-only denoiser (currently)

## Gap Analysis

| Feature | Unreal Lumen | Godot Current | NVIDIA Fork | Aurora Target |
|---|---|---|---|---|
| Diffuse GI | Yes (multi-bounce) | SDFGI (limited) | Yes (path traced) | Yes |
| Specular reflections | Yes | SSR only | Yes (path traced) | Yes |
| Dynamic response | Yes | Limited | Yes | Yes |
| Non-RT hardware | Yes (software RT) | SDFGI/SS effects | No | Yes (hybrid fallback) |
| RT hardware | Yes (acceleration) | No | Yes (required) | Yes |
| Dense geometry | Yes (Nanite) | No | No | Yes (via Meridian) |
| Volumetrics | Yes | Basic | Not yet | Later |
| Virtual shadows | Yes | No | No | Later |

## Key Insight

The NVIDIA fork solves the hardest problem (path tracing) but leaves the broadest problem (hardware reach) unsolved. Lumen's real competitive advantage is not raw quality — it is that it works acceptably on a wide range of hardware. Aurora must address both.

## Competitive Position

### Where Aurora can match Lumen

- lighting quality (path tracing matches or exceeds Lumen on RT hardware)
- dynamic response
- specular reflections

### Where Aurora can exceed Lumen

- physical accuracy (path tracing is more correct than Lumen's hybrid)
- dense geometry lighting via Meridian integration
- simpler mental model for artists (path tracing "just works")

### Where Lumen remains ahead

- non-RT hardware support (Lumen's software RT is mature)
- production maturity and edge case handling
- deep engine integration and tooling
- performance on mid-range hardware
