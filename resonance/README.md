# Project Resonance

Programmable audio system for Godot — a node-based audio graph editor (MetaSounds-class) with a path to LabSound-backed sample-accurate DSP and Steam Audio for spatial audio.

**Status: working prototype.** A GDScript editor plugin provides a visual `GraphEdit`-based editor with five node types running on top of Godot's `AudioServer`. Native LabSound integration is planned but not implemented.

## Layout

```
resonance/
  resonance-plugin/    -- the editor plugin (GDScript)
    nodes/             -- AudioGraphNode subclasses (oscillator, filter, reverb, gain, output)
    graph_editor/      -- GraphEdit-based UI
    test-project/      -- standalone Godot test project
  labsound-research/   -- notes on LabSound for the planned native backend
  steam-audio-test/    -- notes from testing the community Steam Audio GDExtension
  _pre-consolidation/  -- original PROJECT_PLAN
```

The plugin's own README ([`resonance-plugin/README.md`](./resonance-plugin/README.md)) documents node types, properties, and usage.

## Concept

Godot's audio system handles basic playback, buses, and effects. It lacks:

- Programmable audio graph (MetaSounds-equivalent — node-based synthesis and processing).
- Procedural audio generation.
- Advanced spatial audio (HRTF, ambisonics, occlusion-aware propagation).
- Audio-reactive systems.
- Granular synthesis and physical modeling.

The strategy is two-layered:

1. **Spatial audio** is already solved by Steam Audio (Valve, free SDK) plus the existing community Godot port. Resonance does not rebuild HRTF, occlusion, or propagation.
2. **Programmable audio graph** is what Resonance builds — currently as a GDScript prototype on Godot's `AudioServer`, eventually as a native LabSound (BSD-2) backend for sample-accurate DSP.

## Plan

### Tier 1 — Audio Graph (working prototype)
Node-based audio processing graph, oscillators / filters / envelopes / mixers, trigger-based playback, editor UI for graph authoring, GDExtension delivery. Currently a GDScript prototype.

### Tier 2 — Spatial Audio
HRTF-based 3D audio, geometry-aware occlusion and propagation, reverb-zone detection, distance attenuation models. Will use the community Steam Audio port rather than reimplement.

### Tier 3 — Advanced
Procedural audio generation (footsteps, impacts, environments), granular synthesis, physical modeling (string, membrane, resonance), audio analysis output for VFX/gameplay integration.

## Architecture

```
VFX Graph Editor (Godot GraphEdit)
    -> Audio graph description (node connections, parameters)
    -> [current] Godot AudioServer (GDScript)
    -> [planned] LabSound runtime (C++ DSP processing)
    -> Output to Godot AudioServer / direct audio device
```

The current GDScript implementation creates an `AudioServer` bus per source and inserts `AudioEffect*` instances in topological order. Each oscillator drives an `AudioStreamPlayer` with an `AudioStreamGenerator`, with `pump_audio()` called per frame to keep the buffer filled.

A LabSound-backed version is planned — see [`labsound-research/LABSOUND_NOTES.md`](./labsound-research/LABSOUND_NOTES.md) for the API analysis.

## Phase 0 Status

- [x] Test existing Steam Audio Godot GDExtension (community port `stechyo/godot-steam-audio`).
- [x] Prototype GraphEdit-based audio node editor (5 nodes).
- [x] Prototype audio output via Godot `AudioServer`.
- [ ] Clone and build LabSound.
- [ ] Prototype LabSound integration as Godot GDExtension.
- [ ] Test LabSound audio output routed to `AudioServer`.
- [ ] Benchmark LabSound DSP performance for real-time game use.

Exit criteria: Steam Audio spatial audio works in Godot (community port — done); LabSound graph produces audio output routable to Godot (pending); visual graph editor prototype shows node connections working (done).

## Current Limitations

- **Built on Godot AudioServer.** All processing uses Godot's built-in audio effects and bus system. Sample-accurate DSP requires native code.
- **Single chain per source.** Each oscillator routes through one linear chain. Parallel paths, mixing, splitting are not supported.
- **No audio file input.** Only oscillator-generated tones. No `AudioStreamPlayer` / WAV / OGG input.
- **No modulation.** Static parameters. No LFO, envelopes, or parameter automation.
- **No MIDI.** No MIDI input or keyboard triggering.
- **Buses created at runtime.** Adding `AudioServer` buses dynamically can cause brief glitches on first play.
- **LabSound integration planned, not implemented.**

## References

- Steam Audio: https://valvesoftware.github.io/steam-audio/
- Steam Audio Godot GDExtension (community): https://github.com/stechyo/godot-steam-audio
- LabSound: https://github.com/LabSound/LabSound
- Godot `AudioServer`: https://docs.godotengine.org/en/stable/classes/class_audioserver.html
- Godot `GraphEdit`: https://docs.godotengine.org/en/stable/classes/class_graphedit.html
- MetaSounds — Unreal docs and public GDC talks.
