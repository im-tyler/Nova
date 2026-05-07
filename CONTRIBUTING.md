# Contributing

Light System is solo-developed but contributions are welcome. This guide covers what you need to build the C++ extensions and run the demo.

## Prerequisites

- **Godot 4.6+** (Forward+ renderer, or 4.4+ for individual subsystems where noted).
- **C++17 compiler** — Clang or GCC. MSVC 2019+ on Windows.
- **SCons** for the GDExtension builds. Install via `pip install scons` or your package manager.
- **Python 3** for SCons.
- **Git** for cloning vendored upstreams.

Per-subsystem extras:

- **cascade / tempest**: nothing beyond the above; godot-cpp is cloned at build time.
- **aurora**: requires the NVIDIA RTX Godot fork (https://github.com/NVIDIA-RTX/godot). Aurora is research-stage — there is no buildable artifact yet.
- **meridian**: lives in its own repo (http://100.108.123.49:49152/Tyler/meridian.git). Vulkan SDK 1.2+ required for the standalone prototype. See meridian's own README.

## Building

Each C++ subsystem builds independently. From the repository root:

```bash
# Cascade — physics
cd cascade/cascade
git clone --depth 1 https://github.com/godotengine/godot-cpp.git
scons platform=macos target=template_debug -j10

# Tempest — VFX (reuses cascade's godot-cpp)
cd ../../tempest/tempest
ln -sf ../../cascade/cascade/godot-cpp godot-cpp
scons platform=macos target=template_debug -j10
```

Replace `macos` with `linux` or `windows`, and `template_debug` with `template_release` for optimized builds.

## Running the Demo

After building cascade and tempest:

```bash
cd godot-parity-demo
./setup.sh
godot --path .
```

The setup script verifies the GDExtension binaries are reachable. Open the project in the Godot Project Manager if you prefer. The demo scene shows cloth, fluid, fracture, and GPU particles running together in one scene.

## Pull Request Guidelines

- One subsystem per PR is preferable — don't mix changes across atlas, aurora, cascade, etc.
- Be honest about status. If something is a prototype, the README's `Current Limitations` section should reflect that. We will not accept PRs that quietly remove limitations without proving they're fixed.
- If you're adding a new subsystem capability, include a benchmark or demo scene that shows it working.
- C++ style follows Godot's conventions (snake_case for files, PascalCase for classes); GDScript follows the Godot style guide.
- Don't commit binaries (`bin/`, `*.dylib`, `*.so`, `*.dll`) or Godot caches (`.godot/`, `.import/`, `*.uid`).

## Reporting Issues

Issues are best filed on the Forgejo repository. Please include:

- Which subsystem (atlas, aurora, cascade, kinetic, meridian, resonance, scatter, tempest, godot-parity-demo).
- Godot version, OS, GPU.
- Minimum reproduction (a small `.tscn` is ideal).
- Expected vs actual behavior.

## Code of Conduct

Be civil. Critique work, not people. Solo project — turnaround is best-effort.
