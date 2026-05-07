# Project Scatter

Node-based procedural content generation for Godot — Unreal PCG Framework as the parity bar. A `GraphEdit`-based visual editor builds DAGs of `ScatterNode` resources that sample surfaces, filter by slope, randomize transforms, and instance meshes via `MultiMeshInstance3D`.

**Status: working prototype.** The plugin runs end-to-end in the editor: surface sampling, slope filtering, random transforms, and instance placement all work. Sequential graph execution only — branching/merging not supported yet.

## Layout

```
scatter/
  scatter-plugin/      -- the editor plugin (GDScript)
    nodes/             -- ScatterNode subclasses (samplers, filters, modifiers, output)
    graph_editor/      -- GraphEdit-based UI
  test-project/        -- standalone Godot test project
  _pre-consolidation/  -- original PROJECT_PLAN
```

The plugin's own README ([`scatter-plugin/README.md`](./scatter-plugin/README.md)) documents node types, properties, and usage in detail.

## Concept

Godot has no built-in procedural generation framework. Artists place content manually or write custom GDScript. Unreal's PCG Framework provides a node-based graph for procedural rules — surface scatter, rule-based filtering, runtime or editor-time execution. Scatter brings that to Godot.

The framework is a directed acyclic graph where each node transforms a set of points:

```
[Surface Sampler] -> [Slope Filter] -> [Height Filter] -> [Random Transform] -> [Instance Placer]
   input mesh        remove > 45 deg    remove < 10m       offset/rotate/scale     -> MultiMeshInstance3D
```

Node categories:

- **Input** — surface sampler, volume sampler, spline sampler, grid sampler.
- **Filter** — slope, height, distance, density, boolean mask, noise threshold.
- **Transform** — random offset, random rotation, random scale, snap to surface, align to normal.
- **Output** — `MultiMeshInstance3D` placer, `VGeoMeshInstance3D` (meridian) placer, scene instantiator.
- **Combiner** — merge / subtract / intersect point sets.

## Plan

### Tier 1 — Scatter and Place (working prototype)
- Node-based graph editor for placement rules. **Working.**
- Surface sampling on meshes / terrain. **Working.**
- Rule nodes (slope, random transforms). **Working.**
- Output to `MultiMeshInstance3D`. **Working.**
- Editor-time execution with preview. **Working.**
- Branching and merging in the graph. **Not yet — execution is strictly sequential by index.**

### Tier 2 — Advanced Rules
Spline-based placement (along paths, roads, rivers), boolean operations (exclude/blend zones), biome system (layered rule sets by region), runtime execution for dynamic worlds (atlas integration).

### Tier 3 — Generation
Procedural mesh generation (walls, fences, buildings from rules), L-system vegetation, Wave Function Collapse for layout generation, atlas streaming integration (generate on cell load).

## Architecture

Pure GDScript editor plugin — no GPU compute needed for point-set operations at typical sizes. The graph editor uses Godot's `GraphEdit`. Output nodes create standard Godot scene nodes (`MultiMeshInstance3D` today; `VGeoMeshInstance3D` once meridian integration lands).

`ScatterPoint` carries position, normal, and a full `Transform3D` that accumulates modifications as it flows through the graph. Each node's `execute()` receives an array of `ScatterPoint` and returns a (potentially filtered/modified) array. `ScatterGraph` stores an ordered list of nodes; execution is sequential from index 0 to N.

## Phase 0 Status

- [x] Study Unreal 5.7 PCG Framework architecture (public docs).
- [x] Prototype `GraphEdit`-based PCG editor as `EditorPlugin`.
- [x] Implement surface sampler node.
- [x] Implement slope filter node.
- [x] Implement random transform node.
- [x] Implement `MultiMeshInstance3D` output node.
- [x] Test with simple scenario (scatter rocks on terrain).
- [ ] Benchmark — 10K and 100K instance placement speed.
- [ ] Branching / merging in graph execution.

Exit criteria: PCG graph editor works in Godot editor (done); can scatter objects on a surface with filtering (done); live preview updates when graph changes (done).

## Current Limitations

- **Sequential graph only.** The visual editor supports node connections, but execution is strictly sequential (array index order). Branching and merging are not supported.
- **CPU-only.** All sampling and filtering on the CPU. Large point counts (>50k) may cause frame hitches during execution.
- **Single surface.** `SurfaceSampler` reads surface 0 of the assigned mesh only.
- **No undo/redo integration.** Graph edits in the visual editor are not registered with Godot's undo system.
- **No persistence.** The graph is not saved between editor sessions. Saving/loading graph resources is not yet implemented.

## References

- Unreal 5.7 PCG Framework: https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview
- Godot `GraphEdit`: https://docs.godotengine.org/en/stable/classes/class_graphedit.html
- Godot `MultiMeshInstance3D`: https://docs.godotengine.org/en/stable/classes/class_multimeshinstance3d.html
- Houdini procedural workflows (conceptual reference).
- Wave Function Collapse algorithm.
