# Project Resonance

Last updated: 2026-03-24

## Mission

Build a programmable audio system for Godot that competes with Unreal's MetaSounds, delivered as a GDExtension.

## Context

### The Gap

Godot's audio system provides basic playback, buses, and effects. It lacks:

- programmable audio graph (MetaSounds equivalent — node-based audio synthesis and processing)
- procedural audio generation
- advanced spatial audio (HRTF, ambisonics, occlusion-aware propagation)
- audio-reactive systems (feed audio analysis to gameplay or VFX)
- granular synthesis, physical modeling

### Relationship to Other Projects

- **Tempest** VFX can be audio-reactive if audio analysis data is available
- **Cascade** destruction events generate audio triggers
- Spatial audio benefits from scene geometry (occlusion, reverb zones)

## Foundations Available

### Steam Audio (Free SDK, Valve)
- HRTF spatial audio, physics-based occlusion and propagation, reverb baking
- free SDK, no royalties, available for any team size
- **already has Godot GDExtension integrations** (multiple community ports)
- GitHub (Godot port): https://github.com/stechyo/godot-steam-audio
- this solves spatial audio entirely — do not reimplement HRTF, occlusion, or propagation

### LabSound (BSD-2)
- C++ graph-based audio engine derived from WebAudio specification
- cross-platform (RtAudio/miniaudio backends): Windows, macOS, Linux, iOS, Android
- HRTF support, thread-safe for multi-threaded apps
- node-based DSP processing graph
- oscillators, filters, envelopes, analyzers, mixers
- GitHub: https://github.com/LabSound/LabSound
- this is the foundation for the programmable audio graph

## Technical Approach

### Two-Layer Architecture

**Layer 1: Spatial Audio — Steam Audio (already solved)**

Steam Audio provides HRTF, occlusion, physics-based propagation, and reverb baking. Community Godot GDExtensions already exist. Resonance does not rebuild this.

**Layer 2: Programmable Audio Graph — LabSound + Godot integration**

LabSound (BSD-2) provides the DSP graph runtime. Resonance wraps it:

```
VFX Graph Editor (Godot GraphEdit)
    |
    v
Audio graph description (node connections, parameters)
    |
    v
LabSound runtime (C++ DSP processing)
    |
    v
Output to Godot AudioServer (or direct to audio device)
```

Alternatively, if LabSound integration proves too heavy, build a lighter DSP graph directly on Godot's AudioServer using AudioEffectCapture and custom AudioStream implementations.

### What Resonance Actually Builds

Since Steam Audio handles spatial and LabSound handles DSP:

1. Godot GraphEdit-based audio graph editor (visual node editing)
2. Bridge between LabSound graph and Godot AudioServer output
3. Trigger system (game events fire audio graph instances)
4. Procedural audio nodes (impact sounds, footsteps, wind)
5. Audio analysis output for VFX/gameplay integration

### Phase 0: Assessment and Prototype (3-4 weeks)

- [ ] test existing Steam Audio Godot GDExtension (https://github.com/stechyo/godot-steam-audio)
- [ ] evaluate Steam Audio quality and performance in Godot scenes
- [ ] clone and build LabSound
- [ ] prototype LabSound integration as Godot GDExtension
- [ ] test LabSound audio output routed to Godot AudioServer
- [ ] prototype GraphEdit-based audio node editor (3-4 basic nodes)
- [ ] benchmark LabSound DSP performance for real-time game use

Exit criteria:
- Steam Audio spatial audio works in Godot (may already work via community port)
- LabSound graph produces audio output routable to Godot
- visual graph editor prototype shows node connections working

## Product Goal

### Tier 1: Audio Graph

- node-based audio processing graph
- oscillators, filters, envelopes, mixers as graph nodes
- trigger-based playback (events fire audio graph instances)
- editor UI for graph authoring
- GDExtension delivery

### Tier 2: Spatial Audio

- HRTF-based 3D audio
- geometry-aware occlusion and propagation
- reverb zone detection from scene geometry
- distance attenuation models

### Tier 3: Advanced

- procedural audio generation (footsteps, impacts, environments)
- granular synthesis
- physical modeling (string, membrane, resonance models)
- audio analysis output for VFX/gameplay

## Delivery

GDExtension. Extends Godot's existing AudioServer and AudioStreamPlayer rather than replacing them.

## Key References

- Steam Audio: https://valvesoftware.github.io/steam-audio/
- Steam Audio Godot GDExtension: https://github.com/stechyo/godot-steam-audio
- LabSound: https://github.com/LabSound/LabSound
- Godot AudioServer docs
- Godot GraphEdit docs
- MetaSounds architecture (Unreal docs, public GDC talks)
