# Project Forge

Last updated: 2026-03-25

## Mission

Build a universal asset converter and store for Godot that streamlines importing assets from any source — 3D models, textures, materials, animations, complete scenes — across formats and engines. One tool, all conversions, integrated into the Godot editor.

## Context

### The Gap

Godot's built-in importers handle glTF, OBJ, FBX (via ufbx), and Collada, but:

- no unified conversion pipeline for batch processing
- no material translation between engines (Unreal MaterialX, Unity HDRP, Blender Principled BSDF)
- no scene-level conversion (Unreal levels, Unity scenes)
- no asset store integrated into the editor
- no Datasmith-equivalent for architectural/CAD workflows
- texture format conversion is manual

Unreal has:
- Datasmith for CAD/BIM/DCC interchange
- Fab marketplace integrated into the editor
- Robust FBX/USD/Alembic import with material translation

### What Forge Provides

A unified import/conversion/distribution pipeline:

1. **Convert**: drag-and-drop any asset format, auto-detect and convert
2. **Translate**: map materials between engines/DCCs automatically
3. **Batch**: process entire directories or project exports
4. **Store**: browse, preview, and install assets from within the editor
5. **Export**: prepare Godot assets for other engines (reverse pipeline)

## Product Goal

### Tier 1: Universal Converter

- Import: glTF, FBX, OBJ, USD, Alembic, DAE, STL, PLY, 3DS
- Textures: PNG, JPG, EXR, HDR, DDS, KTX2, TIFF, BMP, TGA, WebP
- Materials: Principled BSDF -> Godot StandardMaterial3D mapping
- Animations: skeletal, morph targets, transform animations
- Batch conversion with progress reporting
- CLI mode for pipeline integration

### Tier 2: Material Translation

- Unreal material parameter mapping
- Unity HDRP/URP material parameter mapping
- Blender Principled BSDF parameter mapping
- Substance material import
- Automatic texture channel packing (ORM maps)

### Tier 3: Asset Store

- Browse community/commercial assets from within Godot editor
- Preview 3D models with orbit viewer
- One-click install to project
- Version management and updates
- Integration with Godot Asset Library API

## Technical Approach

GDScript EditorPlugin with optional C++ acceleration for heavy conversion tasks.

### Core Pipeline

```
Input File(s)
    |
    v
Format Detection (magic bytes + extension)
    |
    v
Parser (format-specific: FBX, USD, glTF, etc.)
    |
    v
Intermediate Representation (Godot scene tree + resources)
    |
    v
Material Translation (source engine -> Godot mapping)
    |
    v
Output (Godot .tscn/.tres/.res files)
```

### Key Dependencies

- ufbx (already in Godot) for FBX
- Godot's built-in glTF module
- OpenUSD (for USD support — may need GDExtension)
- KTX-Software for texture compression

## Phase Plan

### Phase 0: Converter Core (4-6 weeks)

- batch import UI in editor
- format detection
- material parameter mapping (Principled BSDF -> StandardMaterial3D)
- texture format conversion pipeline
- CLI interface

### Phase 1: Material Translation (6-8 weeks)

- Unreal material parameter database
- Unity HDRP/URP parameter database
- automatic texture channel detection and repacking

### Phase 2: Asset Store (8-12 weeks)

- editor dock with browse/search/preview
- Godot Asset Library API integration
- 3D preview with orbit controls
- install/update management

## Delivery

GDScript EditorPlugin. C++ GDExtension for heavy lifting (texture compression, mesh optimization) if needed.

## Key References

- Godot Asset Library API
- Unreal Datasmith architecture
- OpenUSD: https://openusd.org
- ufbx: https://github.com/blobber/ufbx
- KTX-Software: https://github.com/KhronosGroup/KTX-Software
- MaterialX: https://materialx.org
