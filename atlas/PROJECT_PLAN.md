# Project Atlas

Last updated: 2026-03-24

## Mission

Build a world streaming and partition system for Godot that enables open-world-scale content, coordinating Meridian's geometry streaming, Aurora's lighting data, and asset loading at world scale.

## Context

### The Gap

Godot's scene system is designed around discrete scenes loaded in full. There is no built-in:

- world partitioning (automatic spatial subdivision of world data)
- level streaming (load/unload world chunks based on camera position)
- data layers (separate world state into streamable layers)
- large-world coordinate support (double precision or origin rebasing for worlds > 10km)
- one-file-per-actor or equivalent for collaborative editing at scale

Unreal's World Partition provides all of this and is essential for open-world games.

### Relationship to Other Projects

Atlas is the conductor that orchestrates streaming for all other systems:

- **Meridian** streams geometry pages — Atlas tells it which regions of the world are relevant
- **Aurora** has lighting data (probes, BVH) that needs streaming for large worlds
- **Cascade** physics simulation needs to know what's active
- **Tempest** VFX needs to activate/deactivate based on streaming state

Without Atlas, the other projects work for room-scale and corridor-scale content but not open worlds.

## Foundations Available

### Open World Database (OWDB)
- Godot addon for camera-based chunk streaming
- automatic chunking, load/unload based on camera position
- multiplayer networking support
- published November 2025, actively maintained
- study this for streaming architecture patterns
- GitHub: https://github.com/DigitallyTailored/Godot-Open-World-Database

### Chunx
- simpler Godot 4 streaming plugin
- WorldStreamer node, automatic chunk load/unload
- GitHub: https://github.com/SlashScreen/chunx

### Godot Large World Coordinates
- already supported in Godot (double precision build)
- meshes stable at billions of units from origin
- documented: https://docs.godotengine.org/en/stable/tutorials/physics/large_world_coordinates.html
- this is NOT a problem Atlas needs to solve -- it's already done

### Godot ResourceLoader
- async resource loading already exists in Godot
- supports background loading with progress callbacks
- Atlas builds on this, doesn't replace it

## Product Goal

### Tier 1: Spatial Streaming

- automatic grid-based world partitioning
- distance-based loading/unloading of world cells
- async loading with priority scheduling
- integration with Meridian's geometry streaming
- basic editor visualization of streaming grid

### Tier 2: Data Layers and Collaboration

- separate world data into layers (base geometry, gameplay objects, foliage, etc.)
- per-layer streaming control
- one-file-per-entity or per-cell serialization for version control friendly workflows
- collaborative editing support (multiple people editing different cells)

### Tier 3: Large World Support

- large-world coordinates (double precision or origin rebasing)
- hierarchical LOD for world-scale content (HLOD generation)
- runtime HLOD streaming
- minimap and overview generation from streaming data

## Technical Approach (Updated)

Atlas is NOT a low-level I/O system. Godot already has async ResourceLoader and large world coordinates. Atlas is a **coordination and policy layer** that:

1. Decides which world cells are relevant (spatial query based on camera, velocity prediction, explicit hints)
2. Tells each subsystem what to load/unload:
   - Meridian: geometry pages for these cells
   - Aurora: lighting data (probes, BVH segments) for these cells
   - Cascade: activate physics simulation for these cells
   - Tempest: activate VFX for these cells
3. Enforces memory budgets across all streaming systems
4. Manages data layers (base geometry, foliage, gameplay objects as separate streamable layers)

## Delivery

GDExtension. Atlas coordinates streaming decisions -- it does not need to modify Godot's core scene tree or resource loading. It uses existing ResourceLoader async APIs and communicates with other projects through their public APIs.

Engine patches are unlikely to be needed. OWDB and Chunx already prove this pattern works as addons.

## Phase Plan

### Phase 0: Feasibility (3-4 weeks)

- [ ] study OWDB architecture and streaming patterns
- [ ] study Chunx for simpler reference
- [ ] test Godot ResourceLoader async loading performance at scale (100+ chunks)
- [ ] prototype grid-based cell system with camera-distance priority
- [ ] prototype streaming manager with memory budget
- [ ] define cell-to-Meridian page mapping interface
- [ ] test with large scene (1000+ objects across 100+ cells)

Exit criteria:
- streaming manager loads/unloads cells without hitching
- memory budget is respected
- Meridian page streaming can be triggered by cell activation

### Phase 1: Core Streaming

- grid partitioning system
- distance-based streaming manager
- async loading with priority
- Meridian geometry streaming integration
- basic editor grid visualization

### Phase 2: Data Layers

- layer system for world data separation
- per-layer streaming control
- serialization format for version control friendly workflows

### Phase 3: Large World

- large-world coordinate solution (origin rebasing or double precision)
- HLOD generation and streaming
- performance optimization for very large worlds

## Key References

- Unreal World Partition docs (public)
- Unreal Level Streaming docs (public)
- Godot ResourceLoader async loading
- Godot scene tree architecture
- Open-world streaming GDC talks (Horizon, Spider-Man, etc.)
- OWDB: https://github.com/DigitallyTailored/Godot-Open-World-Database
- Chunx: https://github.com/SlashScreen/chunx
- Godot Large World Coordinates: https://docs.godotengine.org/en/stable/tutorials/physics/large_world_coordinates.html
- Godot ResourceLoader: https://docs.godotengine.org/en/stable/classes/class_resourceloader.html
