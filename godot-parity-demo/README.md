# Godot-Unreal Parity Initiative -- Full Demo

A unified Godot 4.6 project that loads every system built under the Godot-Unreal Parity initiative and demonstrates them in a single showcase scene.

## Running

```bash
# First time: create plugin symlinks
./setup.sh

# Launch
godot --path /Users/tyler/Documents/godot-unreal-parity/demo-project
```

Or open the project from the Godot Project Manager.

## Active Systems

### GDExtension (C++/Rust native libraries)

| System | Binary | Description | Status |
|---|---|---|---|
| Cascade Cloth | `bin/libcascade...dylib` | GPU cloth simulation (XPBD solver). Red drape, top-pinned with wind. | Built, binary present |
| Cascade Fluid | `bin/libcascade...dylib` | SPH fluid simulation. Blue, 2048 particles in a bounded domain. | Built, binary present |
| Cascade Fracture | `bin/libcascade...dylib` | Voronoi fracture system. Gold box that shatters after 4 seconds. | Built, binary present |
| Tempest VFX | `bin/libtempest...dylib` | GPU particle system rivaling Niagara/VFX Graph. | Built, binary present |

### GDScript Plugins (editor addons)

| System | Path | Description | Status |
|---|---|---|---|
| Scatter | `addons/scatter/` | Procedural content generation framework (PCG Graph equivalent). | Functional, symlinked |
| Resonance | `addons/resonance/` | Visual audio graph editor using Godot AudioServer. | Functional, symlinked |
| Kinetic | `addons/kinetic/` | Motion matching system with inertialized transitions. | Functional, symlinked |

## Scene Layout

The demo scene (`main.tscn`) builds everything at runtime via GDScript:

- **Left** -- Cascade Cloth: red fabric, top-pinned, wind force applied
- **Center** -- Cascade Fluid: blue SPH fluid, 2048 particles
- **Right** -- Cascade Fracture: gold box, Voronoi shatters at t=4s
- **Ground** -- StaticBody3D plane (30x30 units)
- **Camera** -- slow orbit around the scene center
- **Lighting** -- 3-point setup (warm key, cool fill, neutral rim) with shadows on key light
- **Background** -- dark blue-black with ambient fill and glow post-process

If a GDExtension class is not found at runtime, a translucent placeholder mesh is shown instead and the console prints which systems loaded successfully.

FPS is printed to the console every 5 seconds.

## Project Structure

```
demo-project/
  project.godot           -- Godot project config (forward_plus renderer)
  main.tscn               -- Showcase scene with inline GDScript
  cascade.gdextension     -- Points to Cascade physics dylib
  tempest.gdextension     -- Points to Tempest VFX dylib
  setup.sh                -- Creates plugin symlinks
  bin/
    libcascade.macos.template_debug.universal.dylib
    libtempest.macos.template_debug.universal.dylib
  addons/
    scatter/  -> symlink to /Users/tyler/Documents/procgen/scatter-plugin
    resonance/ -> symlink to /Users/tyler/Documents/audio/resonance-plugin
    kinetic/  -> symlink to /Users/tyler/Documents/animation/kinetic-plugin/addons/kinetic
```

## Requirements

- Godot 4.6+
- macOS (universal binary, arm64 + x86_64)
- Source plugin repos must exist at their expected paths for symlinks to resolve
