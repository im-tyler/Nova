# Godot-Unreal Parity Initiative

Closing the critical feature gaps between Godot and Unreal Engine through focused, interconnected projects spanning rendering, lighting, physics, VFX, animation, audio, procedural generation, and asset management.

## Projects

| Project | Codename | Type | Status | Description |
|---|---|---|---|---|
| Dense Geometry | **Meridian** | C++ engine module | Phase 0 | Nanite-competitive virtualized geometry renderer |
| Lighting/GI | **Aurora** | C++ engine module | Research | Lumen-competitive path tracing + hybrid fallback |
| Physics Sim | **Cascade** | C++ GDExtension | Working | GPU cloth (XPBD), fluid (SPH), fracture (Voronoi) |
| VFX/Particles | **Tempest** | C++ GDExtension | Working | GPU particle system with force fields |
| World Streaming | **Atlas** | GDExtension | Planned | World partition and level streaming coordinator |
| Animation | **Kinetic** | GDScript plugin | Prototype | Motion matching with BVH import |
| Audio | **Resonance** | GDScript plugin | Prototype | Programmable audio graph with visual editor |
| Procedural Gen | **Scatter** | GDScript plugin | Prototype | Node-based PCG framework with graph editor |
| Asset Converter | **Forge** | GDScript plugin | Planned | Universal asset converter and store |

## Quick Start

### Run the Demo

```bash
cd demo-project
godot --path .
```

The demo showcases Cascade (cloth + fluid + fracture) running simultaneously.

### Build C++ Extensions

```bash
# Cascade (physics)
cd cascade
git clone --depth 1 https://github.com/godotengine/godot-cpp.git
scons platform=macos target=template_debug -j10

# Tempest (VFX)
cd ../tempest
ln -s ../cascade/godot-cpp godot-cpp
scons platform=macos target=template_debug -j10
```

### Install GDScript Plugins

Copy any plugin folder to your Godot project's `addons/` directory and enable in Project Settings:

```
scatter/   -> addons/scatter/
resonance/ -> addons/resonance/
kinetic/   -> addons/kinetic/
```

## Architecture

All GPU compute uses Godot's RenderingDevice API (Vulkan compute via GLSL shaders compiled to SPIR-V). No CUDA, no vendor lock-in.

```
Rendering (Meridian)
    |
    +---> Lighting (Aurora)
    +---> VFX (Tempest)
    +---> World Streaming (Atlas)

Physics (Cascade)
    |
    +---> VFX (Tempest) -- shared particle buffer
    +---> Animation (Kinetic) -- cloth on skeletons

Independent:
    Scatter (procedural generation)
    Resonance (audio)
    Forge (asset management)
```

## Key Decisions

- **Delivery**: GDExtension-first, no hard fork of Godot
- **GPU compute**: Vulkan via RenderingDevice, portable GLSL shaders
- **No vendor lock-in**: works on NVIDIA, AMD, Intel, Apple Silicon
- **PhysX 5.6** (BSD-3) as algorithmic reference for physics, not runtime dependency
- **NVIDIA RTX Godot fork** (MIT) as foundation for Aurora path tracing
- **Intel OIDN** (Apache 2.0) as vendor-agnostic denoiser

## Documentation

- [Overview](docs/OVERVIEW.md) -- full initiative plan with dependency map
- [Synthesis](docs/SYNTHESIS.md) -- deep analysis, gap assessment, concrete plans
- [Research Papers](docs/RESEARCH_PAPERS.md) -- 10 analyzed papers with integration priorities
- Each project has its own README.md with build/install/usage instructions

## License

Individual project licenses vary. See each project's directory for details.
Core initiative code: MIT unless otherwise noted.
