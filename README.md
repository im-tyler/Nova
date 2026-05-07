# Light System

A Godot-Unreal Parity Initiative. Light System is an umbrella for nine subsystems that work to close the feature gap between Godot and Unreal Engine across rendering, lighting, physics, VFX, animation, audio, world streaming, and procedural generation.

This repository is the umbrella — the planning hub plus a unified showcase project. Each subsystem lives in its own folder and ships independently as either a C++ GDExtension, a GDScript editor plugin, or (for one subsystem) a separate Forgejo repository.

## Subsystems

| Codename | Role | Status | Type | Coupling |
|----------|------|--------|------|----------|
| **meridian** | Dense / virtualized geometry renderer (Nanite-class) | working (Phase 2 in progress) | C++ standalone Vulkan + Godot importer | Lives in [its own repo](https://github.com/im-tyler/meridian.git) — not vendored here |
| **aurora** | Path-traced GI + hybrid fallback (Lumen-class) | research-only | Built on the [NVIDIA RTX Godot fork](https://github.com/NVIDIA-RTX/godot) | Foundation cloned into `aurora/nvidia-godot-rtx/` (gitignored) |
| **cascade** | GPU physics — XPBD cloth, SPH fluid, Voronoi fracture | working (prototype) | C++ GDExtension (godot-cpp + RenderingDevice compute) | Standalone — depends on godot-cpp |
| **tempest** | GPU particles / VFX (Niagara-class) | working (prototype) | C++ GDExtension | Shares particle buffer format with cascade (planned) |
| **atlas** | World partition and streaming coordinator | planned-only | TBD GDExtension | Coordinates meridian, aurora, cascade, tempest at world scale |
| **kinetic** | Motion matching + procedural animation | working (prototype) | GDScript editor plugin | Reads Skeleton3D; cloth attached to bones via cascade |
| **resonance** | Programmable audio graph (MetaSounds-class) | working (prototype) | GDScript editor plugin | Standalone — bridges Godot AudioServer |
| **scatter** | Node-based PCG framework (PCG-class) | working (prototype) | GDScript editor plugin | Standalone — outputs MultiMeshInstance3D |
| **godot-parity-demo** | Unified showcase loading every shipping subsystem | working | Godot 4.6 project | Demo only |

Honest status taxonomy:

- **research-only** — published research notes only; no implementation.
- **planned-only** — has a project plan; no implementation yet.
- **prototype** — node types or addons exist and run, with documented limitations.
- **working** — runs end-to-end on at least one platform with measured performance.
- **shipping** — none of the subsystems claim this yet.

## Quick Start

### Run the showcase demo (macOS)

```bash
cd godot-parity-demo
./setup.sh         # verifies binaries are present
godot --path .
```

The demo scene loads cascade (cloth + fluid + fracture), tempest (GPU particles), and the three GDScript plugins (scatter, resonance, kinetic) in a single Godot project. If a GDExtension binary is missing, a placeholder mesh is shown and the console reports which subsystems loaded.

You will need to build the C++ GDExtensions first — see the per-subsystem READMEs for instructions.

### Build the C++ GDExtensions

Each C++ subsystem is a standalone SCons build that depends on `godot-cpp`. Cascade's checkout of godot-cpp is the canonical one; tempest historically symlinked to it.

```bash
# Cascade — cloth, fluid, fracture
cd cascade/cascade
git clone --depth 1 https://github.com/godotengine/godot-cpp.git
scons platform=macos target=template_debug -j10

# Tempest — GPU particles
cd ../../tempest/tempest
ln -sf ../../cascade/cascade/godot-cpp godot-cpp
scons platform=macos target=template_debug -j10
```

Replace `macos` with `linux` or `windows` and `template_debug` with `template_release` as needed.

### Install GDScript plugins

The three GDScript plugins live under each subsystem's `*-plugin/` directory. Copy the plugin folder into your Godot project's `addons/`:

```
kinetic/kinetic-plugin/addons/kinetic/   -> your_project/addons/kinetic/
resonance/resonance-plugin/              -> your_project/addons/resonance/
scatter/scatter-plugin/                  -> your_project/addons/scatter/
```

Enable them in Project Settings > Plugins.

## Architecture

All GPU compute targets Godot's RenderingDevice API (Vulkan compute via GLSL shaders compiled to SPIR-V). No CUDA, no vendor lock-in. The intent is to run on NVIDIA, AMD, Intel, and Apple Silicon.

```
Rendering (meridian)
    +---> Lighting (aurora)
    +---> VFX (tempest)
    +---> World Streaming (atlas)

Physics (cascade)
    +---> VFX (tempest)        -- shared particle buffer
    +---> Animation (kinetic)  -- cloth on skeletons

Independent:
    scatter   (procedural generation)
    resonance (audio)
```

## Key Decisions

- **Delivery vehicle**: GDExtension first. Engine module / fork only when GDExtension cannot reach the necessary engine internals (currently meridian is the most likely candidate).
- **GPU compute API**: Godot RenderingDevice (Vulkan compute) on the primary path. Portable GLSL shaders compilable to SPIR-V.
- **No vendor lock-in**: NVIDIA-only paths (CUDA, DLSS) are acceptable as premium options but never the default.
- **Algorithmic references** (study only, not runtime dependencies): PhysX 5.6 (BSD-3) for physics solvers; NVIDIA Blast (Nvidia Source Code License) for fracture; Intel OIDN (Apache 2.0) as the cross-vendor denoiser.
- **Foundation reuse**: aurora is built on the [NVIDIA RTX Godot fork](https://github.com/NVIDIA-RTX/godot) (MIT) rather than reinventing the path tracer.

## Vendored Upstreams

The following upstreams are cloned locally for build/research but are **not** committed to this repository (they live in their own repos under their own licenses):

- `aurora/nvidia-godot-rtx/` — [NVIDIA-RTX/godot](https://github.com/NVIDIA-RTX/godot) (MIT)
- `cascade/blast-research/Blast/` — [NVIDIAGameWorks/Blast](https://github.com/NVIDIAGameWorks/Blast) (Nvidia Source Code License)
- `cascade/cascade/godot-cpp/` — [godotengine/godot-cpp](https://github.com/godotengine/godot-cpp) (MIT)

Clone them yourself before building. See per-subsystem READMEs.

## Repository Layout

```
atlas/              project plan only — no implementation yet
aurora/             research notes + path to NVIDIA fork
cascade/            cascade/ (gdextension), test-project/, blast-research/
godot-parity-demo/  unified showcase Godot project
kinetic/            kinetic-plugin/ (addon), reference/, test-data/
meridian/           SEPARATE REPO — see https://github.com/im-tyler/meridian.git
resonance/          resonance-plugin/, labsound-research/, steam-audio-test/
scatter/            scatter-plugin/, test-project/
tempest/            tempest/ (gdextension), test-project/
```

Each subsystem's README covers its own concept, current status, build, and limitations. The `_pre-consolidation/` folder inside each subsystem preserves the original PROJECT_PLAN / ARCHITECTURE_DECISIONS / COMPETITIVE_ANALYSIS / IMPLEMENTATION_BACKLOG / PHASE0_CHECKLIST documents that were collapsed into the consolidated README.

## Contributing

- License: MIT for project-authored code unless otherwise noted. See [LICENSE](LICENSE).
- Code style: small surface, honest status, no oversold marketing.
- Each subsystem's `Current Limitations` section is the truth — not the wishlist.
- Pull requests welcome on any subsystem. Issues for missing features are fine, but the project is solo-developed and turnaround is best-effort.

See [CONTRIBUTING.md](CONTRIBUTING.md) for build prerequisites and how to run the demo.

## License

MIT. See [LICENSE](LICENSE).

Individual subsystems may carry their own license files for code derived from upstreams (NVIDIA RTX Godot fork, godot-cpp, etc.). Honor those licenses where they apply.
