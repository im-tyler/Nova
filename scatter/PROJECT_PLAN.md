# Project Scatter

Last updated: 2026-03-24

## Mission

Build a node-based procedural content generation framework for Godot that competes with Unreal's PCG Framework, delivered as a GDExtension.

## Context

### The Gap

Godot has no built-in procedural generation framework. Artists and level designers manually place content or write custom GDScript. Unreal's PCG Framework provides:

- node-based graph for procedural rules
- scatter points on surfaces (rocks, foliage, props)
- rule-based placement (slope, height, distance, density)
- runtime or editor-time execution
- integration with landscape and world partition

### Relationship to Other Projects

- **Atlas** world streaming determines what regions need generated content
- **Meridian** dense geometry is a natural output of procedural placement (many instances)
- **Tempest** VFX foliage/grass can be placed procedurally

## Foundations Available

### No open-source PCG framework exists for game engines

This is original work. References:

- Unreal 5.7 PCG Framework (now production-ready) — study architecture via public docs and GDC talks
- Houdini procedural workflows — conceptual reference for node-based generation

### Godot GraphEdit
- existing node graph UI control
- used by VisualShader, AnimationTree
- this is the UI framework for the PCG graph editor

### Godot MultiMeshInstance3D
- efficient instanced rendering for thousands of identical meshes
- natural output target for scattered objects (rocks, foliage, props)
- Meridian's VGeoMeshInstance3D extends this for dense geometry instances

## Technical Approach

### Point Set Operations Graph

The PCG framework is a directed acyclic graph (DAG) where each node transforms a set of points:

```
[Surface Sampler] → [Slope Filter] → [Height Filter] → [Random Transform] → [Instance Placer]
     input mesh       remove > 45°     remove < 10m      offset/rotate/scale   → MultiMeshInstance3D
```

Node categories:
- **Input nodes**: surface sampler, volume sampler, spline sampler, grid sampler
- **Filter nodes**: slope, height, distance, density, boolean mask, noise threshold
- **Transform nodes**: random offset, random rotation, random scale, snap to surface, align to normal
- **Output nodes**: MultiMeshInstance3D placer, VGeoMeshInstance3D placer, scene instantiator
- **Combiner nodes**: merge point sets, subtract point sets, intersect

### Execution Modes

- **Editor-time**: generate in editor, bake to scene. Preview updates live as graph changes.
- **Runtime**: generate on load, useful with Atlas streaming (generate content when cell loads)

### Implementation

Pure GDScript/C++ EditorPlugin. No GPU compute needed — point set operations are lightweight. The graph editor uses Godot's GraphEdit. Output nodes create standard Godot scene nodes.

### Phase 0: Prototype (3-4 weeks)

- [ ] study Unreal 5.7 PCG Framework architecture (public docs)
- [ ] prototype GraphEdit-based PCG editor as EditorPlugin
- [ ] implement surface sampler node (scatter points on mesh surface)
- [ ] implement slope filter node
- [ ] implement random transform node
- [ ] implement MultiMeshInstance3D output node
- [ ] test with simple scenario: scatter rocks on terrain
- [ ] benchmark: 10K instances placement speed

Exit criteria:
- PCG graph editor works in Godot editor
- can scatter objects on a surface with filtering
- live preview updates when graph changes

## Product Goal

### Tier 1: Scatter and Place

- node-based graph editor for placement rules
- surface sampling (scatter points on meshes, terrain)
- rule nodes: slope filter, height filter, distance filter, density control, random offset/rotation/scale
- output: MultiMeshInstance3D, or Meridian VGeoMeshInstance3D for dense content
- editor-time execution with preview
- GDExtension delivery

### Tier 2: Advanced Rules

- spline-based placement (along paths, roads, rivers)
- boolean operations (exclude zones, blend zones)
- biome system (layered rule sets by region)
- runtime execution for dynamic worlds

### Tier 3: Generation

- procedural mesh generation (walls, fences, buildings from rules)
- L-system vegetation
- wave function collapse for layout generation
- integration with Atlas streaming (generate on load)

## Delivery

GDExtension. Pure tooling layer — no engine changes needed. Graph editor as an EditorPlugin.

## Key References

- Unreal 5.7 PCG Framework: https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview
- Godot GraphEdit: https://docs.godotengine.org/en/stable/classes/class_graphedit.html
- Godot MultiMeshInstance3D: https://docs.godotengine.org/en/stable/classes/class_multimeshinstance3d.html
- Houdini procedural workflows (conceptual reference)
- Wave Function Collapse algorithm
