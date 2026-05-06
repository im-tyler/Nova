# LabSound Research Notes for Project Resonance

Repository: https://github.com/LabSound/LabSound
License: BSD 2-Clause (permissive, suitable for commercial use)
Language: C++14/17
Origin: Forked from WebKit's WebAudio implementation, extended significantly

---

## 1. API Architecture

LabSound follows the WebAudio specification's graph-based model with significant C++ improvements.

### AudioContext

The central orchestrator. All node creation, connection, and lifecycle management flows through it.

```cpp
// Two modes: realtime and offline
AudioContext(bool isOffline);
AudioContext(bool isOffline, bool autoDispatchEvents);
```

Key responsibilities:
- Owns the destination node (audio output sink)
- Manages the graph topology (connect/disconnect)
- Drives the render loop via render quantums
- Provides timing (currentTime, currentSampleFrame, predictedCurrentTime)
- Holds the AudioListener for 3D spatialization
- Handles HRTF database loading for binaural audio
- Event dispatch system (enqueueEvent / dispatchEvents)

Lifecycle: `lazyInitialize()` -> active -> `suspend()` / `resume()` -> `close()`

For offline rendering: `startOfflineRendering()` with `offlineRenderCompleteCallback`.

### AudioNode

Abstract base class for all processing units. Every node has:
- Named inputs and outputs (indexed or string-addressed)
- AudioParams (automatable float parameters, a-rate or k-rate)
- AudioSettings (non-automatable configuration values)
- Channel configuration (count, count mode, interpretation)
- A scheduling state machine (UNSCHEDULED -> SCHEDULED -> PLAYING -> FINISHED)

Required overrides for custom nodes:
```cpp
virtual const char* name() const;
virtual void process(ContextRenderLock&, int bufferSize);
virtual void reset(ContextRenderLock&);
virtual double tailTime(ContextRenderLock&);
virtual double latencyTime(ContextRenderLock&);
```

### AudioParam

Parameters support full WebAudio automation timeline:
- `setValueAtTime(value, time)`
- `linearRampToValueAtTime(value, time)`
- `exponentialRampToValueAtTime(value, time)`
- `setTargetAtTime(target, time, timeConstant)`
- `setValueCurveAtTime(curve, time, duration)`
- `cancelScheduledValues(startTime)`
- Built-in de-zippering (smoothing) to prevent clicks on value changes
- Can be driven by other AudioNodes via `connectParam()`

### Graph Connection

```cpp
// Node-to-node
ac.connect(destination, source, destIdx, srcIdx);
ac.disconnect(destination, source, destIdx, srcIdx);

// Node-to-parameter (audio-rate modulation)
ac.connectParam(param, driverNode, channelIndex);

// Fluent operator>> syntax
oscillator >> gain >> ac;  // chains to destination
lfo >> gain->gain();       // modulates a parameter
```

### Node Registry

A singleton `NodeRegistry` allows runtime discovery and dynamic instantiation:
- `NodeRegistry::Instance().Register(name, descriptor, createFn, deleteFn)`
- `NodeRegistry::Instance().Create(name, audioContext)`
- `NodeRegistry::Instance().Names()` -- enumerate all registered types
- `NodeRegistry::Instance().Descriptor(name)` -- introspect params/settings

This is crucial for a visual graph editor -- nodes can be listed and instantiated by name string.

---

## 2. Available Node Types

### Core Nodes (WebAudio-derived)

| Node | Purpose |
|------|---------|
| OscillatorNode | Waveform generator: SINE, FAST_SINE, SQUARE, SAWTOOTH, FALLING_SAWTOOTH, TRIANGLE, CUSTOM. Params: frequency, detune, amplitude, bias |
| GainNode | Volume control with automatic de-zippering. Param: gain |
| BiquadFilterNode | Standard biquad filter. Types: LOWPASS, HIGHPASS, BANDPASS, LOWSHELF, HIGHSHELF, PEAKING, NOTCH, ALLPASS. Params: frequency, q, gain, detune. Has getFrequencyResponse() |
| DelayNode | Simple delay line |
| DynamicsCompressorNode | Dynamics compressor. Params: threshold, knee, ratio, attack, release, reduction |
| ConvolverNode | Convolution reverb via impulse response. setImpulse()/getImpulse(). Thread-safe kernel swapping |
| PannerNode | 3D spatial audio. Models: EQUALPOWER, HRTF. Distance models: LINEAR, INVERSE, EXPONENTIAL. Full position/orientation/velocity. Cone attenuation. Doppler |
| StereoPannerNode | Simple L/R stereo panning |
| AnalyserNode | FFT-based analysis. getFloatFrequencyData (dB), getByteFrequencyData, getFloatTimeDomainData, getByteTimeDomainData. Configurable FFT size, smoothing, dB range |
| ChannelSplitterNode | Split multi-channel into mono outputs |
| ChannelMergerNode | Merge mono inputs into multi-channel |
| ConstantSourceNode | Outputs a constant value (useful for parameter automation) |
| WaveShaperNode | Arbitrary waveshaping distortion curve |
| SampledAudioNode | Sample playback with scheduling. Params: playbackRate, detune, dopplerRate. Methods: schedule(), start() with loopCount, grainOffset, grainDuration. getCursor() for playback position |
| AudioHardwareInputNode | Microphone/line input capture |
| MixNode | Mix/sum multiple inputs |

### Extended Nodes (LabSound originals)

| Node | Purpose |
|------|---------|
| ADSRNode | Attack-Decay-Sustain-Release envelope. Params: gate, oneShot, attackTime, attackLevel, decayTime, sustainTime, sustainLevel, releaseTime |
| FunctionNode | Arbitrary DSP callback. Signature: `void(ContextRenderLock&, FunctionNode*, int channel, float* buffer, int bufferSize)`. Access to `now()` for time |
| NoiseNode | White, Pink, Brown noise generators |
| PolyBLEPNode | Anti-aliased oscillator with 12 waveforms: TRIANGLE, SQUARE, RECTANGLE, SAWTOOTH, RAMP, MODIFIED_TRIANGLE, MODIFIED_SQUARE, HALF_WAVE_RECTIFIED_SINE, FULL_WAVE_RECTIFIED_SINE, TRIANGULAR_PULSE, TRAPEZOID_FIXED, TRAPEZOID_VARIABLE |
| SupersawNode | Multiple detuned sawtooth oscillators. Params: sawCount, frequency, detune |
| SfxrNode | Retro game sound generator (SFXR port). 8 presets: beep, coin, laser, explosion, powerUp, hit, jump, select. ~25 synthesis parameters. mutate() and randomize() |
| GranulationNode | Granular synthesis. Params: numGrains, grainDuration, grainPositionMin/Max, grainPlaybackFreq. Subsample-accurate timing with crossfade windowing |
| ClipNode | Hard clipping (threshold) or soft clipping (tanh saturation) |
| DiodeNode | Vacuum tube diode distortion (extends WaveShaperNode). Params: distortion, vb, vl |
| PWMNode | Pulse width modulation. Two inputs: carrier + modulator |
| BPMDelayNode | Tempo-synced delay. SetTempo(), SetDelayIndex(TempoSync) |
| PingPongDelayNode | Stereo ping-pong delay with tempo sync, feedback, wet/dry. Uses Subgraph pattern with internal splitter/merger |
| PeakCompNode | Stereo peak compressor. Params: threshold, ratio, attack, release, makeup, knee |
| RecorderNode | Records audio to memory. writeRecordingToWav(), createBusFromRecording(). Thread-safe via recursive mutex |
| PowerMonitorNode | RMS power measurement in dB. Good for VU meters and ducking |
| SpectralMonitorNode | Real-time spectral magnitude analysis. spectralMag() fills a vector |

---

## 3. Creating and Connecting a Graph

### Typical pattern from examples:

```cpp
// 1. Get or create AudioContext (examples use a shared base class)
auto& ac = *_context.get();

// 2. Create nodes
auto osc = std::make_shared<OscillatorNode>(ac);
auto gain = std::make_shared<GainNode>(ac);

// 3. Configure
osc->frequency()->setValue(440.f);
osc->setType(OscillatorType::SINE);
gain->gain()->setValue(0.5f);

// 4. Connect graph
ac.connect(gain, osc);                        // osc -> gain
ac.connect(ac.destinationNode(), gain);        // gain -> output

// 5. Schedule and start
osc->start(0);  // start immediately

// 6. Synchronize (waits for graph changes to take effect)
ac.synchronizeConnections();
```

### FM Synthesis example pattern:
```cpp
auto carrier = std::make_shared<OscillatorNode>(ac);
auto modulator = std::make_shared<OscillatorNode>(ac);
auto modGain = std::make_shared<GainNode>(ac);

ac.connect(modGain, modulator);
ac.connectParam(carrier->frequency(), modGain, 0);  // modulate frequency
ac.connect(ac.destinationNode(), carrier);
```

### Offline rendering pattern:
```cpp
// Use AudioDevice_Null backend
auto recorder = std::make_shared<RecorderNode>(ac, outConfig);
recorder->startRecording();
// ... build graph connected to recorder ...
ac.startOfflineRendering();
// In callback:
recorder->stopRecording();
recorder->writeRecordingToWav("output.wav", false);
```

### Fluent operator>> syntax:
```cpp
oscillator >> gain >> ac;     // connect chain to destination
lfo >> gain->gain();          // modulate parameter
```

---

## 4. Audio Output: Backends, Buffer Sizes

### Backend Architecture

AudioDevice is the abstract base class. Three implementations:

| Backend | Class | Platforms | Notes |
|---------|-------|-----------|-------|
| RtAudio | AudioDevice_RtAudio | macOS, Windows, Linux | Default on macOS/Linux. Mature, well-tested. Wraps platform APIs (CoreAudio, WASAPI, ALSA, JACK, PulseAudio) |
| miniaudio | AudioDevice_Miniaudio | All platforms | Default on Windows/iOS/Android. Single-header C library. 64-byte SIMD alignment on MSVC |
| Mock/Null | AudioDevice_Null | All | No hardware output. For offline rendering and testing |

### Device Enumeration

```cpp
auto devices = AudioDevice_RtAudio::MakeAudioDeviceList();
// or
auto devices = AudioDevice_Miniaudio::MakeAudioDeviceList();
```

Returns `vector<AudioDeviceInfo>` with:
- index, identifier (name string)
- num_output_channels, num_input_channels
- supported_samplerates, nominal_samplerate
- is_default_output, is_default_input

### Configuration

```cpp
AudioStreamConfig outputConfig;
outputConfig.device_index = deviceInfo.index;
outputConfig.desired_channels = 2;
outputConfig.desired_samplerate = 48000.f;

AudioStreamConfig inputConfig;  // similar for mic input

auto device = std::make_shared<AudioDevice_RtAudio>(inputConfig, outputConfig);
```

### Buffer/Quantum Size

LabSound processes audio in render quantums (inherited from WebAudio, typically 128 frames). The backend requests larger buffers from the OS and the internal render loop fills them quantum-by-quantum via `pull_graph()`.

The `SamplingInfo` struct tracks:
- current_sample_frame (uint64)
- current_time (double)
- sampling_rate (float)
- epoch timestamps for drift measurement

### AudioDestinationNode

Bridges the device and the graph. Created by the context, it calls `render()` for realtime or `offlineRender()` for non-realtime. The device callback drives the render pull.

---

## 5. Thread Safety Model

LabSound uses a dual-mutex RAII lock model:

### ContextGraphLock
- Protects the audio graph structure (topology changes: connect/disconnect/add/remove nodes)
- Acquired for graph mutations
- Must NOT be held during audio rendering

### ContextRenderLock
- Protects audio rendering operations
- Acquired per render quantum by the audio thread
- Passed to `process()`, `reset()`, `tailTime()`, `latencyTime()` on every node

### Rules
1. Graph topology changes happen on the main/UI thread under ContextGraphLock
2. Audio processing happens on the audio thread under ContextRenderLock
3. The two locks are never held simultaneously (prevents deadlock)
4. `synchronizeConnections(timeOut_ms)` blocks the calling thread until pending graph changes are applied by the audio thread
5. Node creation and parameter value changes (setValue) are safe from any thread due to atomic operations and smoothing
6. AudioParam automation timeline is lock-free for the audio thread reader
7. Debug mode detects reentrant lock acquisition (appends '~' markers)
8. RecorderNode uses its own recursive mutex for recorded data access

### Practical implications for Godot integration:
- Graph topology changes (connect/disconnect) must be dispatched to a single thread or serialized
- Parameter automation (setValue, ramps) can be called from Godot's game thread safely
- The audio callback thread is managed by the backend (RtAudio/miniaudio), separate from Godot's audio thread

---

## 6. Build Requirements and Dependencies

### Compiler
- C++14 minimum (cmake specifies C++17 in practice)
- MSVC, Clang, GCC all supported

### Build System
- CMake (standard build)
- Clone with `--recursive` for libnyquist submodule

### Required Dependencies
- **libnyquist** (bundled as submodule) -- audio file I/O (WAV, OGG, MP3, FLAC, etc.)
- **libsamplerate** (bundled internally) -- sample rate conversion

### Platform Libraries

**macOS:**
- Cocoa, Accelerate (FFT), CoreAudio, AudioUnit, AudioToolbox frameworks
- FFT: Apple Accelerate vDSP

**Windows:**
- dsound.lib, dxguid.lib, winmm.lib (for RtAudio WASAPI path)
- FFT: KISS FFT (bundled)

**Linux:**
- pthread, dl
- One of: libasound2-dev (ALSA), libpulse-dev (PulseAudio), libjack-dev (JACK)
- FFT: KISS FFT (bundled)

**iOS:**
- Same Apple frameworks as macOS
- Cross-compile via cmake ios-toolchain

**Android:**
- OpenSLES
- Optional JACK/PulseAudio/ALSA

### Build Commands
```bash
git clone --recursive https://github.com/LabSound/LabSound.git
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=../install ..
cmake --build . --target install --config Release
```

Output: `liblabsound.a` (macOS/Linux) or `labsound.lib` (Windows) static library, plus all headers.

---

## 7. Wrapping as a Godot GDExtension

### Architecture Plan

```
[Godot GDExtension (.gdextension)]
    |
    +-- C++ wrapper classes (godot-cpp bindings)
    |       |
    |       +-- ResonanceContext : RefCounted
    |       +-- ResonanceNode : RefCounted (base for all node wrappers)
    |       +-- ResonanceOscillator : ResonanceNode
    |       +-- ResonanceGain : ResonanceNode
    |       +-- ResonanceFilter : ResonanceNode
    |       +-- ... (one wrapper per LabSound node type)
    |
    +-- LabSound static library (linked in)
    +-- libnyquist static library (linked in)
```

### Key Integration Points

1. **AudioContext Lifecycle**
   - Create LabSound AudioContext in GDExtension `_init()` or on explicit user call
   - The LabSound audio thread runs independently of Godot's audio server
   - Option A: LabSound drives its own output (bypasses Godot audio, direct to hardware)
   - Option B: Use AudioDevice_Null + custom pull, pipe samples into Godot AudioStreamGenerator (adds latency but integrates with Godot's audio bus system)
   - Option A is better for low-latency music/sound design tools; Option B for game integration

2. **Node Wrapping Strategy**
   - Use NodeRegistry to enumerate all available nodes at startup
   - Each LabSound node becomes a GDScript-accessible class
   - Expose AudioParams as Godot properties with `_get_property_list()` for dynamic introspection
   - AudioParam automation methods exposed as GDScript methods: `set_value_at_time()`, `linear_ramp_to()`, etc.

3. **Graph Connections**
   - Expose `connect(src, dst, src_idx, dst_idx)` and `disconnect()` on ResonanceContext
   - Parameter connections via `connect_param(node, param_name, driver)`
   - Serialize graph as Godot Resource (.tres) for save/load

4. **Thread Safety**
   - LabSound handles its own audio thread; Godot calls happen on main thread
   - All graph mutations (connect/disconnect) go through LabSound's synchronizeConnections()
   - Parameter changes are safe from any thread (atomic + smoothing)
   - No need to bridge Godot's audio thread if using Option A

5. **Build System**
   - GDExtension uses SCons (godot-cpp) or CMake
   - Link LabSound and libnyquist as static libraries
   - Platform-specific: link CoreAudio frameworks on macOS, WASAPI libs on Windows, ALSA/Pulse on Linux
   - Ship as single shared library per platform (.dylib/.dll/.so)

6. **HRTF Data**
   - Ship IRCAM HRTF WAV files in the Godot project's resource directory
   - Pass path to `loadHrtfDatabase()` on context init

### GDExtension Skeleton

```
resonance-gdextension/
  src/
    register_types.cpp
    resonance_context.h/cpp     -- wraps lab::AudioContext
    resonance_node.h/cpp        -- base wrapper for lab::AudioNode
    resonance_oscillator.h/cpp  -- wraps lab::OscillatorNode
    resonance_gain.h/cpp        -- wraps lab::GainNode
    ...
  thirdparty/
    LabSound/                   -- static lib build
  SConstruct or CMakeLists.txt
  resonance.gdextension         -- Godot extension manifest
```

---

## 8. What We Need to Build on Top

### A. Godot GraphEdit Visual Editor

A visual node graph editor for building audio graphs at edit-time and runtime.

**Requirements:**
- Custom GraphEdit-based scene in Godot
- Each LabSound node type becomes a GraphNode with:
  - Input/output ports matching LabSound's input/output count
  - Knobs/sliders for each AudioParam (frequency, gain, etc.)
  - Dropdowns for AudioSettings (filter type, oscillator type, etc.)
  - Real-time visualization (waveform on oscillators, spectrum on analysers, VU on power monitors)
- Connection rules enforced (type-safe, channel-count validation)
- Serialize entire graph as a custom Godot Resource
- Live preview: changes to graph immediately update the running LabSound context
- Presets/templates: common subgraphs (synth voice, reverb send, sidechain compressor)

**Implementation approach:**
- Use NodeRegistry.Names() to auto-generate the node palette
- NodeRegistry.Descriptor(name) to auto-generate ports and parameter UI
- Store graph as JSON or .tres with node positions, connections, and param values

### B. Trigger System

A system to fire audio events from game logic, animations, or sequencers.

**Requirements:**
- `ResonanceTrigger` class that can:
  - Start/stop scheduled source nodes (oscillators, samples, noise)
  - Gate ADSR envelopes (set gate param to 1.0 then 0.0)
  - Schedule parameter automation (ramps, curves) at specific times
  - Respond to Godot signals, AnimationPlayer keyframes, or Area3D enter/exit
- `ResonanceSequencer`:
  - Timeline with trigger events at beat-quantized positions
  - BPM-aware timing (leverage BPMDelayNode's tempo model)
  - Pattern-based: define patterns of triggers, chain into arrangements
  - MIDI input support (map MIDI notes to triggers)
- `ResonanceMixer`:
  - Godot-side representation of a mixing console
  - Channels map to gain nodes in the graph
  - Solo/mute/send routing
  - Master output with metering (PowerMonitorNode)

### C. Additional Infrastructure Needed

1. **Audio File Manager** -- Load samples via libnyquist, cache as AudioBus, expose to GDScript as resources
2. **Preset System** -- Save/load individual node configurations and full graph snapshots
3. **Parameter Binding** -- Bind any game variable (health, speed, distance) to an AudioParam for reactive audio
4. **Spatialization Bridge** -- If using PannerNode with HRTF, sync position from Godot Node3D transforms to PannerNode position/orientation every frame
5. **Debug/Profiling** -- Expose LabSound's built-in profiling (graphTime, totalTime per node) and diagnose() to Godot editor
6. **Offline Bounce** -- Render graph to WAV using AudioDevice_Null + RecorderNode for non-realtime export

---

## Summary

LabSound is a strong foundation for Project Resonance. It provides:
- A mature, WebAudio-compatible audio graph with 30+ node types
- Full parameter automation with sample-accurate timing
- Clean C++ API with shared_ptr ownership and RAII locking
- Permissive BSD license
- Cross-platform backends (RtAudio, miniaudio)
- A node registry system that maps directly to visual editor needs
- Offline rendering for bounce/export
- Spatial audio with HRTF

The main work is the GDExtension wrapper layer, the Godot GraphEdit visual editor, and the trigger/sequencer system. LabSound handles the DSP; we handle the UX.
