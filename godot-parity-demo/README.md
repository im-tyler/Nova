# Godot-Unreal Parity Demo

Unified Godot 4.6 project that loads every shipping Light System subsystem (cascade, tempest, kinetic, resonance, scatter) into a single showcase scene.

This is part of the [Light System umbrella](../README.md) — see the top-level README for the broader project context.

## Running

```bash
# Verify GDExtension binaries are present
./setup.sh

# Launch
godot --path .
```

Or open the project from the Godot Project Manager.

## Active Subsystems

### GDExtension (C++ native libraries)

| Subsystem | Binary | Description | Status |
|---|---|---|---|
| Cascade Cloth | `bin/libcascade...dylib` | GPU XPBD cloth simulation. Red drape, top-pinned with wind. | Built, binary present |
| Cascade Fluid | `bin/libcascade...dylib` | SPH fluid simulation. Blue, 2048 particles in a bounded domain. | Built, binary present |
| Cascade Fracture | `bin/libcascade...dylib` | Voronoi fracture. Gold box that shatters at t=4s. | Built, binary present |
| Tempest VFX | `bin/libtempest...dylib` | GPU particle system. | Built, binary present |

### GDScript Plugins (editor addons)

| Subsystem | Path | Description | Status |
|---|---|---|---|
| Scatter | `addons/scatter/` | Procedural content generation framework. | Functional |
| Resonance | `addons/resonance/` | Visual audio graph editor. | Functional |
| Kinetic | `addons/kinetic/` | Motion matching with inertialized transitions. | Functional |

## Scene Layout

`main.tscn` builds the scene at runtime via inline GDScript:

- **Left** — Cascade Cloth: red fabric, top-pinned, wind force applied.
- **Center** — Cascade Fluid: blue SPH fluid, 2048 particles.
- **Right** — Cascade Fracture: gold box, Voronoi shatter at t=4s.
- **Ground** — `StaticBody3D` plane (30x30 units).
- **Camera** — slow orbit around the scene center.
- **Lighting** — three-point setup (warm key, cool fill, neutral rim) with shadows on the key light.
- **Background** — dark blue-black with ambient fill and glow post-process.

If a GDExtension class is not found at runtime, a translucent placeholder mesh is shown instead and the console prints which subsystems loaded successfully.

FPS is printed to the console every 5 seconds.

## Project Structure

```
godot-parity-demo/
  project.godot           -- Godot project config (Forward+ renderer)
  main.tscn               -- showcase scene with inline GDScript
  cascade.gdextension     -- points to Cascade physics dylib
  tempest.gdextension     -- points to Tempest VFX dylib
  setup.sh                -- verifies binary presence and addon contents
  bin/                    -- GDExtension binaries (gitignored)
    libcascade.macos.template_debug.universal.dylib
    libtempest.macos.template_debug.universal.dylib
  addons/
    scatter/              -- copied from ../scatter/scatter-plugin/
    resonance/            -- copied from ../resonance/resonance-plugin/
    kinetic/              -- copied from ../kinetic/kinetic-plugin/addons/kinetic/
```

The `addons/` here are real copies of the source plugins, not symlinks. To refresh them, copy the latest plugin contents from each subsystem's `*-plugin/` directory.

## Requirements

- Godot 4.6+
- macOS (the included binaries are universal arm64 + x86_64).

For Linux or Windows, build the cascade and tempest GDExtensions for your platform (see the umbrella [CONTRIBUTING.md](../CONTRIBUTING.md)) and place the resulting `bin/lib*.{so|dll}` here.

## Building Binaries Locally

If `bin/` is empty (it is gitignored), build cascade and tempest from source:

```bash
# From the umbrella repo root
cd cascade/cascade
git clone --depth 1 https://github.com/godotengine/godot-cpp.git
scons platform=macos target=template_debug -j10
cp bin/* ../../godot-parity-demo/bin/

cd ../../tempest/tempest
ln -sf ../../cascade/cascade/godot-cpp godot-cpp
scons platform=macos target=template_debug -j10
cp bin/* ../../godot-parity-demo/bin/
```
