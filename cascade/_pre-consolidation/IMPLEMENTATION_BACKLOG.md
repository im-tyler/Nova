# Physics Sim: Implementation Backlog

Last updated: 2026-03-24

Prioritized for cloth first, fluid second.

## P0: Research and Prototype

- GPU XPBD prototype in compute shaders
- performance benchmarks
- Godot RenderingDevice compute validation
- integration point mapping
- constraint system design

## P1: Cloth Solver Core

- GPU XPBD solver via RenderingDevice compute
- distance constraints
- bending constraints
- attachment constraints
- collision with convex shapes
- collision with triangle meshes
- spatial hashing for broad-phase collision
- time step integration and stability tuning

## P2: Cloth Integration

- ClothBody3D node
- skeletal mesh binding
- vertex painting tool for constraint weights
- wind force field support
- gravity and custom forces
- editor preview
- mesh rendering from simulation output
- export/import constraint maps

## P3: Cloth Polish

- self-collision
- backstop constraints
- LOD (distance-based constraint reduction)
- constraint presets (silk, leather, heavy fabric, etc.)
- performance optimization
- stability edge cases
- documentation

## P4: Fluid Solver Core

- GPU SPH solver
- spatial hashing for neighbor search
- pressure force
- viscosity force
- surface tension
- boundary handling with physics bodies
- particle emission and absorption

## P5: Fluid Integration

- FluidBody3D node
- surface reconstruction (marching cubes or screen-space)
- fluid rendering (screen-space as fast path)
- material properties (density, viscosity, color)
- physics body interaction
- editor preview
- emitter/absorber nodes

## P6: Advanced

- Meridian dense geometry collision for cloth/fluid
- Aurora lighting interaction (wet surfaces, caustics)
- FLIP/APIC solver upgrade for large-scale fluid
- two-way rigid body coupling
- hair simulation prototype
- VFX system integration for fluid rendering
