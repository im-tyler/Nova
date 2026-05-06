# Steam Audio Godot Integration -- Assessment for Project Resonance

Research date: 2026-03-25
Repository: https://github.com/stechyo/godot-steam-audio
License: MIT (extension) / Apache 2.0 (Steam Audio SDK)
Current release: v0.3.1 (December 28, 2024)
Underlying SDK: Steam Audio v4.8.0

---

## Supported Godot Version

- **Godot 4.4+** (works with official Godot 4.4 release, no custom fork required since v0.3.0)
- Earlier releases targeted Godot 4.1 through 4.3 with custom Godot builds; that requirement is gone.

## Platform Support

| Platform | Status |
|----------|--------|
| Windows  | Fully working |
| Linux    | Fully working |
| macOS    | Added in v0.3.1, previously unsupported |
| Android  | Added in v0.3.1 |
| iOS      | Not supported |
| Web      | Not supported |

---

## Nodes / Classes Provided

The extension exposes 9 classes:

### Core (required in every scene)

1. **SteamAudioConfig** -- Singleton-like node, exactly one per scene. Controls global simulation settings: scene type (default/Embree), HRTF volume, reflection threading, max sources, ambisonics order, occlusion samples, logging level. Nothing works without this node.

2. **SteamAudioServer** -- Autoloaded singleton managing all global state. Not meant for direct interaction; the extension manages it internally.

### Listener

3. **SteamAudioListener** -- Placed as child of Camera3D (typically). Exactly one per scene, no multi-listener support. Properties: reflection_duration, irradiance_min_distance, reflection_rays, reflection_bounces, reflection_ambisonics_order. Several properties can be overridden globally by SteamAudioConfig.

### Audio Sources

4. **SteamAudioPlayer** -- Replaces AudioStreamPlayer3D for sources needing spatial effects. Properties:
   - `distance_attenuation` (bool) -- Inverse distance model; disables Godot built-in attenuation when enabled
   - `min_attenuation_distance` (float)
   - `occlusion` (bool) -- Toggles occlusion + transmission
   - `occlusion_radius` (float)
   - `occlusion_samples` (int)
   - `transmission_rays` (int)
   - `ambisonics` (bool) -- Spatial encoding
   - `ambisonics_order` (int)
   - `reflection` (bool)
   - `max_reflection_distance` (float)
   - Methods: `play_stream()`, `get_inner_stream()`, `get_inner_stream_playback()`
   - Runtime stream changes must use `play_stream()`, not `set_stream()`.

5. **SteamAudioStream** -- Internal stream wrapper. Auto-assigned by the extension; users should never assign it manually.

6. **SteamAudioStreamPlayback** -- Internal playback handler. Applies configured effects in the audio thread via `_mix()`.

### Geometry

7. **SteamAudioGeometry** -- Attaches to MeshInstance3D or CollisionShape3D (box, cylinder, capsule, sphere, concave polygon). Extracts mesh at `_ready()` and registers it with the Steam Audio scene. Has `recalculate()` for occasional transform updates. Best for static geometry.

8. **SteamAudioDynamicGeometry** -- Same concept but designed for objects that move every frame. Supports MeshInstance3D and CollisionShape3D (box, cylinder, capsule, sphere -- no concave polygon). Higher CPU cost.

### Materials

9. **SteamAudioMaterial** -- Godot translation of IPLMaterial. Defines acoustic absorption, transmission, and scattering coefficients per surface. Includes presets ported from Valve's Unity plugin.

---

## Steam Audio Features Exposed

| Feature | Status | Notes |
|---------|--------|-------|
| HRTF spatialization | Yes | Built-in, configurable volume via SteamAudioConfig |
| Custom HRTF profiles | No | Open issue #86, not implemented |
| Distance attenuation | Yes | Inverse distance model, replaces Godot's built-in |
| Occlusion | Yes | Ray-based, configurable samples and radius |
| Transmission through geometry | Yes | Configurable ray count per source |
| Ambisonics encoding | Yes | Configurable order |
| Real-time reflections / reverb | Yes | Raycasted, multi-bounce, threaded simulation |
| Air absorption | Yes | Added in v0.3.0 |
| Dynamic geometry | Yes | Dedicated node, added in v0.2.0 |
| Static geometry | Yes | Core feature since v0.1 |
| Embree ray tracing backend | Yes | Optional, faster than default; may crash with other GDExtensions |
| Baked reflections / probes | No | Not implemented |
| Pathing / propagation simulation | No | Open issue #37, not implemented |
| Directivity modeling | No | Open issue #124, not implemented |
| FMOD integration | No | Open issue #65 |
| C# bindings | No | Open issue #99, GDScript only |

---

## Build / Install Requirements

### Pre-built (recommended)
1. Download latest release from GitHub releases page.
2. Copy the `addons/godot-steam-audio` folder into your Godot project.
3. Requires Godot 4.4+.

### Build from source
- **Build system:** SCons
- **Language:** C++ (89.9% of codebase)
- **Dependencies:**
  - Steam Audio SDK libraries in `src/lib/steamaudio`
  - Platform binaries (libphonon.so / phonon.dll) in `project/addons/godot-steam-audio/bin`
  - godot-cpp (GDExtension bindings)
- **Build command:** `scons target=template_debug debug_symbols=true`
- **Code style:** clang-format enforced
- **Refer to Makefile** in repo root for dependency fetch examples.

### Mesh import pipeline
- GLTF scenes can auto-generate SteamAudioGeometry nodes using naming suffixes:
  - `-sasg` for static geometry
  - `-sadg` for dynamic geometry
  - Combinable with Godot import hints (e.g., `-sasg-col`)
- CollisionShape3D is recommended over MeshInstance3D for geometry sources (fewer polygons).

### Export
- As of v0.3.0+ with official Godot 4.4, standard export templates should work.
- Older versions required custom Godot export templates.

---

## Known Limitations

### Stability
- **Alpha-quality software.** The maintainer states: "This extension is in an alpha phase, will have bugs and missing polish, and may crash."
- Crashes are now rare at runtime per the wiki, but edge cases exist.

### Architectural issues
- Runtime creation of SteamAudioPlayer nodes fails to receive effects (#105).
- Removing and re-adding scenes with Steam Audio geometry breaks effects (#102).
- Short audio files cause reflection mixing to stop prematurely (#121).
- Effects break after prolonged use (#75).
- Adding geometry nodes after `_ready()` is unsupported.
- Potential architectural shift from stream-based to effect-based processing identified (#40) but not implemented.

### Feature gaps
- No multi-listener support (exactly one SteamAudioListener required).
- No baked reflections or probe-based reverb (real-time only, CPU-intensive).
- No pathing simulation (sound cannot route around obstacles via portals/openings).
- No directivity modeling (all sources are omnidirectional).
- No custom HRTF loading.
- No C# support.
- No editor audio preview -- sounds don't play in the editor (#39).
- HRTF volume adjustment reportedly non-functional (#97).
- Embree scene type may conflict with other GDExtensions (#107).

### Platform
- iOS and web not supported.
- macOS and Android are new additions (v0.3.1) and may have rough edges.

---

## Threading Model

Three threads:
1. **Audio thread** -- `SteamAudioStreamPlayback::_mix()` applies effects to audio buffers.
2. **Scene thread** -- `SteamAudioServer::tick()` updates simulation for listener and all players against the geometry scene.
3. **Simulation thread** -- Dedicated reflection computation (following Valve's recommendation).

This means deletion order during `queue_free()` and shutdown is non-deterministic across threads. Contributing code must be thread-safe.

---

## How This Fits Into Project Resonance

### What godot-steam-audio handles
- **Core spatial audio pipeline:** HRTF spatialization, distance attenuation, ambisonics encoding. This is the foundation of any physics-based audio sim.
- **Occlusion and transmission:** Ray-based sound blocking and material-dependent transmission through walls/objects. Works out of the box with geometry nodes.
- **Real-time reflections / reverb:** Multi-bounce raycasted reverb. Expensive but functional for small-to-medium scenes.
- **Air absorption:** Frequency-dependent distance falloff. Added in v0.3.0.
- **Material system:** Per-surface acoustic properties (absorption, transmission, scattering) with presets.
- **Dynamic geometry:** Moving objects can affect the acoustic simulation in real time.

### What Project Resonance still needs to build

1. **Pathing / propagation system.** Steam Audio's pathing feature is not exposed in this extension. Sound cannot route through doorways, around corners, or through connected spaces. This is critical for realistic indoor acoustics and will need a custom solution -- either a portal/zone graph system or integration with Steam Audio's pathing API directly via C++ extension work.

2. **Baked acoustics / probe system.** No baked reflection data or acoustic probes. Every reflection is real-time raycasted, which won't scale for large or complex environments. Project Resonance needs a baking pipeline for offline computation of impulse responses or probe-based reverb fields.

3. **Custom HRTF support.** The extension uses Steam Audio's default HRTF. For a project focused on audio realism, personalized or selectable HRTF profiles are important. This requires either upstream support or direct SDK integration.

4. **Directivity modeling.** All sources are omnidirectional. Real instruments, speakers, and voices have radiation patterns. Needs IPLDirectivity integration or a custom wrapper.

5. **Editor tooling.** No audio preview in the editor. For an audio-focused project, rapid iteration requires hearing results without launching the game. May need custom editor plugin work.

6. **Runtime dynamic source management.** Creating/destroying SteamAudioPlayer nodes at runtime is buggy. If Project Resonance has spawned sound sources (e.g., projectiles, environmental triggers), this is a blocker that needs patching or workaround.

7. **Multi-listener support.** If Project Resonance targets multiplayer or split-screen, the single-listener limitation is a hard constraint.

8. **Performance scaling strategy.** Real-time reflections on every source won't scale. Need LOD-style audio: full simulation nearby, simplified fallback at distance, baked data for static environments.

### Recommendation

Use godot-steam-audio as the spatial audio foundation. It covers HRTF, occlusion, transmission, basic reflections, and material acoustics with minimal setup. Build on top of it:
- Implement a portal/zone propagation graph for pathing (the biggest gap).
- Add a baking pipeline for reflection probes in static environments.
- Wrap directivity as a custom property on SteamAudioPlayer or fork the extension.
- Monitor the repo -- it's actively maintained and the maintainer is responsive to contributions.

The extension is alpha but functional. For a research/prototype phase, it provides enough to validate acoustic simulation concepts without writing a spatial audio engine from scratch.
