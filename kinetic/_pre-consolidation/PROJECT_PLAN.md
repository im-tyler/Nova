# Project Kinetic

Last updated: 2026-03-24

## Mission

Build an advanced procedural animation system for Godot that competes with Unreal's Control Rig and Motion Matching, delivered as a GDExtension.

## Context

### The Gap

Godot has AnimationTree with state machines and blend trees — solid for basic animation. But it lacks:

- procedural animation system (full-body IK, physics-driven motion, runtime pose manipulation)
- motion matching (data-driven animation selection from motion capture databases)
- runtime retargeting (share animations across different skeleton proportions)
- Control Rig equivalent (visual scripting for skeletal manipulation)
- secondary motion (jiggle bones, dynamic bone chains beyond basic SpringBoneSimulator)

### Relationship to Other Projects

- **Cascade** cloth simulation interacts with skeletal animation (cloth attached to bones)
- **Cascade** physics-driven animation reads rigid body state
- Animation drives the skeletons that Cascade's cloth attaches to

## Foundations Available

### orangeduck/Motion-Matching (canonical reference)
- by Daniel Holden, the researcher who invented Learned Motion Matching
- C++ implementation with source for "Code vs Data Driven Displacement"
- this is the PhysX 5.6 equivalent for animation — the authoritative open reference
- GitHub: https://github.com/orangeduck/Motion-Matching

### Open-Source-Motion-Matching-System
- C++ rewrite of Unreal's motion matched animation sample project
- useful for understanding how motion matching integrates with an engine's animation system
- GitHub: https://github.com/dreaw131313/Open-Source-Motion-Matching-System

### SIGGRAPH Asia 2025: Environment-aware Motion Matching
- recent academic work extending motion matching with environment awareness
- code available alongside the paper

### Mesh2Motion
- open source Mixamo alternative
- auto-rigging and control rig generation
- supports bipeds, quadrupeds, birds

### Godot AnimationTree
- Godot's existing animation system with state machines and blend trees
- Kinetic extends this, doesn't replace it
- new node types plug into the existing AnimationTree framework

## Product Goal

### Tier 1: Procedural Animation

- full-body IK solver (FABRIK or similar, GPU-accelerated for many characters)
- physics-driven animation (ragdoll blending, hit reactions)
- runtime pose modification (look-at, foot placement, hand placement)
- dynamic bone chains (hair, tails, accessories)
- GDExtension delivery

### Tier 2: Motion Matching

- motion database format and builder
- motion matching search (GPU-accelerated for large databases)
- blending and transition system
- editor tools for motion database inspection

### Tier 3: Advanced

- runtime retargeting across skeleton proportions
- visual scripting for skeletal manipulation (Control Rig equivalent)
- crowd animation (many characters with shared motion matching)
- facial animation system

## Delivery

GDExtension. Extends Godot's existing Skeleton3D and AnimationTree rather than replacing them. Compute shaders via RenderingDevice for GPU-accelerated IK and motion matching on many characters.

### Phase 0: Research and Prototype (4-6 weeks)

- [ ] study orangeduck/Motion-Matching implementation in detail
- [ ] study Unreal's motion matching sample via Open-Source-Motion-Matching-System
- [ ] define motion database format (compatible with common mocap formats)
- [ ] prototype motion matching search on CPU (single character)
- [ ] benchmark search cost vs animation database size
- [ ] assess GPU-accelerated search for crowd scenarios (100+ characters)
- [ ] prototype FABRIK IK solver integration with Skeleton3D
- [ ] prototype ragdoll blend (AnimationTree -> Jolt ragdoll -> blend back)
- [ ] define AnimationTree node interface for new node types

Exit criteria:
- motion matching works for a single character with basic locomotion
- IK foot placement works on uneven terrain
- ragdoll blend transitions look acceptable

## Key References

- orangeduck/Motion-Matching: https://github.com/orangeduck/Motion-Matching
- Open-Source-Motion-Matching-System: https://github.com/dreaw131313/Open-Source-Motion-Matching-System
- Mesh2Motion: https://gamefromscratch.com/mesh2motion-open-source-mixamo-alternative/
- Daniel Holden's publications on motion matching
- GDC Motion Matching talks (Ubisoft, Naughty Dog)
- FABRIK IK algorithm paper
- Godot Skeleton3D docs
- Godot AnimationTree docs
