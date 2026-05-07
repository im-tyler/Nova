# Project Cascade

GPU physics engine for Godot 4.4+, built as a GDExtension (C++ / godot-cpp). Cascade provides XPBD cloth, SPH fluid, and Voronoi fracture simulation, all driven by compute shaders dispatched through Godot's RenderingDevice API.

Part of the Godot-Unreal Parity Initiative.

## Node Types

| Node | Base Class | Purpose |
|------|-----------|---------|
| `CascadeWorld` | Node3D | Coordinator node. Manages substep timing for all Cascade solvers in the scene. IMEX time splitting: cloth at 1/60s, fluid at 1/120s, fracture event-driven. |
| `CascadeCloth` | MeshInstance3D | GPU XPBD cloth simulation. Supports grid-generated and arbitrary source meshes. Constraint graph coloring for parallel solves. Sphere and plane colliders. |
| `CascadeFluid` | MultiMeshInstance3D | SPH fluid simulation. Particles rendered as instanced spheres via MultiMesh. GPU spatial hashing with bitonic sort and prefix-sum grid construction. |
| `CascadeFracture` | MeshInstance3D | Voronoi fracture. Pre-fractures mesh at setup time. `apply_damage()` separates pieces as RigidBody3D nodes with impulse. |
| `CascadeComputeTest` | MeshInstance3D | Diagnostic node. Validates the compute-to-mesh pipeline by dispatching a simple wave shader and rendering the result. |

## Build

Requires: SCons, a C++ compiler with C++17 support, Python 3.

```bash
cd cascade/cascade
git clone --depth 1 https://github.com/godotengine/godot-cpp.git
cd godot-cpp && scons platform=macos target=template_debug -j10 && cd ..
scons platform=macos target=template_debug -j10
```

The build produces a framework bundle at:

```
bin/libcascade.macos.template_debug.framework
```

Replace `macos` with `linux` or `windows` for other platforms. Replace `template_debug` with `template_release` for optimized builds.

## Install

1. Copy the `bin/` directory into your Godot project root.
2. Copy `cascade.gdextension` into your Godot project root (next to `project.godot`).
3. Open the project in Godot 4.4+. The five node types will appear in the Add Node dialog.

Directory layout inside your project:

```
your_project/
  project.godot
  cascade.gdextension
  bin/
    libcascade.macos.template_debug.framework
```

## Usage

### Cloth

```
1. Add a CascadeWorld node to your scene.
2. Add a CascadeCloth node as a child.
3. Configure properties in the inspector:
   - width / height: grid resolution (default 32x32)
   - spacing: distance between vertices (default 0.08)
   - iterations: XPBD solver iterations (default 10, higher = stiffer)
   - pin_mode: 0 = top row pinned, adjustable per-vertex via pin_vertex()
   - wind: wind direction vector
   - wind_turbulence: randomized wind variation
   - stretch_compliance / bend_compliance: constraint softness (0 = rigid)
4. Set simulate = true to start.
```

For arbitrary mesh input, assign a `source_mesh` in the inspector or via script. The cloth solver extracts edges from the mesh topology and simulates them as distance constraints.

Colliders are added via script:

```gdscript
$CascadeCloth.add_sphere_collider(Vector3(0, -1, 0), 0.5)
$CascadeCloth.add_plane_collider(Vector3.UP, -2.0)
```

### Fluid

```
1. Add a CascadeFluid node under CascadeWorld.
2. Set num_particles (default 2048), smoothing_radius, rest_density.
3. Set bounds_min / bounds_max to define the simulation domain.
4. Set simulate = true.
```

Particles are rendered as instanced spheres. Particle radius controls visual size; smoothing radius controls SPH kernel range.

### Fracture

```
1. Add a CascadeFracture node with a mesh assigned.
2. Set num_pieces and fracture_seed.
3. Call fracture() at runtime (or it runs automatically in _ready).
4. Call apply_damage(point, radius, force) to break pieces off.
```

Separated pieces become RigidBody3D nodes with ConvexPolygonShape3D colliders.

## Architecture

All GPU work runs on a local RenderingDevice instance (not the main renderer's device). Each solver compiles GLSL compute shaders at init time, allocates GPU buffers, and dispatches work groups per frame. Results are read back to the CPU for mesh updates.

The CascadeWorld node coordinates timing via `_physics_process()`. Cloth runs at `cloth_substeps` per physics frame (default 1), fluid runs at `fluid_substeps` (default 2, giving 120Hz at 60fps physics).

## Current Limitations

- **CPU readback for mesh update.** Every frame, vertex positions are read back from GPU to CPU to update the ArrayMesh/MultiMesh. This is the main performance bottleneck.
- **No self-collision.** Cloth does not detect or resolve self-intersection.
- **Basic Voronoi fracture.** Fracture uses a simplified Voronoi decomposition. No support for pre-scored fracture patterns, hierarchical fracture, or runtime re-fracture of pieces.
- **No inter-solver coupling.** Cloth and fluid do not interact with each other. Each solver operates independently.
- **Collider types are limited.** Cloth supports sphere and plane colliders only. No mesh colliders.
- **No XPBD long-range attachment constraints.** Only distance and bending constraints are implemented.
- **SPH boundary handling.** Fluid uses simple position clamping at domain bounds rather than proper boundary particles.
