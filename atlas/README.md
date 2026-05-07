# Project Atlas

World streaming and partition coordinator for Godot. Atlas is the conductor that decides which world cells are active and tells meridian, aurora, cascade, and tempest what to load and unload.

**Status: planned-only.** No implementation exists. The original project plan is preserved in [`_pre-consolidation/PROJECT_PLAN.md`](./_pre-consolidation/PROJECT_PLAN.md).

## Concept

Godot's scene system loads discrete scenes in full. There is no built-in world partitioning, distance-based level streaming, data-layer separation, or HLOD generation at world scale. Unreal's World Partition provides all of this and is essential for open-world games. Atlas closes that gap.

Atlas is **not** a low-level I/O system. Godot already has async `ResourceLoader` and large-world coordinates. Atlas is a **coordination and policy layer** that:

1. Decides which world cells are relevant (camera distance, velocity prediction, explicit hints).
2. Tells each subsystem what to load and unload — meridian gets geometry pages, aurora gets lighting data, cascade activates physics, tempest activates VFX.
3. Enforces memory budgets across all streaming systems.
4. Manages data layers (base geometry, foliage, gameplay) as separately streamable layers.

## Plan

### Tier 1 — Spatial Streaming
Grid-based world partitioning, distance-based load/unload, async loading with priority scheduling, integration with meridian's geometry streaming, basic editor visualization.

### Tier 2 — Data Layers and Collaboration
Layer separation (base / gameplay / foliage), per-layer streaming control, one-file-per-cell or one-file-per-entity serialization for version-control-friendly workflows.

### Tier 3 — Large World
Origin rebasing or double-precision support, hierarchical LOD generation (HLOD), runtime HLOD streaming, minimap and overview generation.

## Architecture

GDExtension. Atlas does not need to modify Godot's core scene tree or resource loading. It uses existing `ResourceLoader` async APIs and communicates with other subsystems through their public APIs. Engine patches are unlikely to be needed — both [Open World Database](https://github.com/DigitallyTailored/Godot-Open-World-Database) and [Chunx](https://github.com/SlashScreen/chunx) prove the pattern works as an addon.

## Foundations Available

- **Open World Database (OWDB)** — Godot addon for camera-based chunk streaming. Studied for streaming architecture patterns. https://github.com/DigitallyTailored/Godot-Open-World-Database
- **Chunx** — simpler Godot 4 streaming plugin with WorldStreamer node. https://github.com/SlashScreen/chunx
- **Godot Large World Coordinates** — already supported in double-precision builds. Atlas does not need to solve this. https://docs.godotengine.org/en/stable/tutorials/physics/large_world_coordinates.html
- **Godot ResourceLoader** — async resource loading with progress callbacks. Atlas builds on this, doesn't replace it.

## Phase 0 Checklist

- [ ] Study OWDB architecture and streaming patterns.
- [ ] Study Chunx for a simpler reference.
- [ ] Test Godot `ResourceLoader` async loading at scale (100+ chunks).
- [ ] Prototype grid-based cell system with camera-distance priority.
- [ ] Prototype streaming manager with memory budget.
- [ ] Define cell-to-meridian-page mapping interface.
- [ ] Test with a large scene (1000+ objects across 100+ cells).

Exit criteria: streaming manager loads/unloads cells without hitching, memory budget is respected, meridian page streaming can be triggered by cell activation.

## Relationship to Other Subsystems

- **meridian** streams geometry pages — atlas tells it which world regions are relevant.
- **aurora** streams lighting data (probes, BVH segments) — atlas drives the streaming policy.
- **cascade** activates physics simulation per cell.
- **tempest** activates VFX per cell.

Without atlas, the other subsystems work for room-scale and corridor-scale content. Open worlds need this coordinator.

## References

- Unreal World Partition documentation (public).
- Unreal Level Streaming documentation (public).
- Open-world streaming GDC talks (Horizon, Spider-Man, etc.).
- Godot `ResourceLoader`: https://docs.godotengine.org/en/stable/classes/class_resourceloader.html
