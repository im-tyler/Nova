# Project Scatter

Procedural content generation framework for Godot 4.4+. A GDScript EditorPlugin that provides a visual graph editor for building and executing ScatterNode pipelines -- sample surfaces, filter by slope, randomize transforms, and instance meshes.

Part of the Godot-Unreal Parity Initiative.

## Install

1. Copy the `scatter-plugin/` directory into your project's `addons/` folder:

```
your_project/
  addons/
    scatter/
      plugin.cfg
      scatter_plugin.gd
      scatter_graph.gd
      scatter_point.gd
      nodes/
        scatter_node.gd
        surface_sampler.gd
        slope_filter.gd
        random_transform.gd
        instance_placer.gd
      graph_editor/
        scatter_graph_editor.gd
        scatter_graph_node_ui.gd
        node_palette.gd
```

2. In Godot, go to Project > Project Settings > Plugins.
3. Enable "Scatter".

The plugin adds a "Scatter" bottom panel containing a GraphEdit-based visual editor.

## Node Types

The pipeline is a directed acyclic graph of `ScatterNode` resources. Points flow from generators through filters and transforms to output nodes.

### SurfaceSampler (generator)

Samples random points on a MeshInstance3D surface, weighted by triangle area.

| Property | Type | Description |
|----------|------|-------------|
| `point_count` | int | Number of points to generate (default 100) |
| `seed` | int | Random seed for deterministic output |
| `mesh_path` | NodePath | Path to the target MeshInstance3D |

### SlopeFilter (filter)

Removes points whose surface normal falls outside a slope angle range.

| Property | Type | Description |
|----------|------|-------------|
| `min_angle` | float | Minimum slope angle in degrees (0 = flat ground) |
| `max_angle` | float | Maximum slope angle in degrees (90 = vertical wall) |

### RandomTransform (modifier)

Applies random offset, rotation, and scale to each point.

| Property | Type | Description |
|----------|------|-------------|
| `offset_range` | Vector3 | Max random offset per axis |
| `rotation_range` | Vector3 | Max random rotation per axis in degrees (default: Y=360) |
| `scale_range` | Vector2 | Uniform scale min/max (default: 0.8 to 1.2) |
| `seed` | int | Random seed |

### InstancePlacer (output)

Creates a MultiMeshInstance3D with one instance per point.

| Property | Type | Description |
|----------|------|-------------|
| `mesh` | Mesh | The mesh to instance at each point |
| `material_override` | Material | Optional material override |

## Usage

### Visual Editor

1. Open the "Scatter" bottom panel.
2. Click "Add Node" to add nodes from the dropdown (Surface Sampler, Slope Filter, Random Transform, Instance Placer).
3. Connect nodes by dragging between ports in the graph editor.
4. Click "Execute" to run the pipeline. The status bar shows point count and elapsed time.
5. Click "Clear" to remove all nodes.

### Script

```gdscript
var graph := ScatterGraph.new()

var sampler := SurfaceSampler.new()
sampler.point_count = 500
sampler.seed = 42
sampler.mesh_instance = $Terrain  # MeshInstance3D reference

var slope := SlopeFilter.new()
slope.min_angle = 0.0
slope.max_angle = 30.0

var transform := RandomTransform.new()
transform.rotation_range = Vector3(0, 360, 0)
transform.scale_range = Vector2(0.6, 1.4)

var placer := InstancePlacer.new()
placer.mesh = preload("res://tree.tres")
placer.parent = $ScatterRoot  # Node3D to parent the MultiMeshInstance3D under

graph.add_node(sampler)
graph.add_node(slope)
graph.add_node(transform)
graph.add_node(placer)

var points := graph.execute()
# placer has created a MultiMeshInstance3D under $ScatterRoot
```

## Data Model

`ScatterPoint` carries position (Vector3), normal (Vector3), and a full Transform3D that accumulates modifications as it flows through the graph. Each node's `execute()` receives an array of ScatterPoint and returns a (potentially filtered/modified) array.

`ScatterGraph` stores an ordered list of nodes. Execution is sequential from index 0 to N -- the output of each node feeds into the next.

## Current Limitations

- **Sequential graph only.** The visual editor supports connections between nodes, but execution is strictly sequential (array index order). Branching and merging are not supported.
- **CPU-only.** All sampling and filtering runs on the CPU. Large point counts (>50k) may cause frame hitches during execution.
- **Single surface.** SurfaceSampler reads surface 0 of the assigned mesh only.
- **No undo/redo integration.** Graph edits in the visual editor are not registered with Godot's undo system.
- **No persistence.** The graph is not saved between editor sessions. Saving/loading graph resources is not yet implemented.
