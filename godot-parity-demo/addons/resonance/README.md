# Project Resonance

Programmable audio graph for Godot 4.4+. A GDScript EditorPlugin that provides a visual node-based editor for building audio processing chains on top of Godot's AudioServer. Create oscillators, route through filters, reverb, and gain nodes, and play the result in real time.

Part of the Godot-Unreal Parity Initiative.

## Install

1. Copy the `resonance-plugin/` directory into your project's `addons/` folder:

```
your_project/
  addons/
    resonance/
      plugin.cfg
      resonance_plugin.gd
      audio_graph.gd
      nodes/
        audio_graph_node.gd
        oscillator_node.gd
        filter_node.gd
        reverb_node.gd
        gain_node.gd
        output_node.gd
      graph_editor/
        audio_graph_editor.gd
```

2. In Godot, go to Project > Project Settings > Plugins.
3. Enable "Resonance Audio Graph".

The plugin adds an "AudioGraph" dock (bottom-right by default) with a GraphEdit-based visual editor.

## Node Types

All nodes extend `AudioGraphNode` (a Resource subclass). Each node declares its input/output port count and optionally returns an `AudioEffect` for bus insertion.

### OscillatorNode (source)

Generates a waveform via AudioStreamGenerator. This is the sound source -- it drives an AudioStreamPlayer.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `frequency` | float | 440.0 | Tone frequency in Hz |
| `waveform` | enum | SINE | SINE, SQUARE, SAW, or TRIANGLE |
| `amplitude` | float | 0.5 | Output amplitude (0.0 to 1.0) |

### FilterNode (effect)

Wraps Godot's AudioEffectFilter. Supports lowpass, highpass, and bandpass modes.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `filter_type` | enum | LOWPASS | LOWPASS, HIGHPASS, or BANDPASS |
| `cutoff_frequency` | float | 1000.0 | Cutoff frequency in Hz (20 to 20000) |
| `resonance` | float | 0.5 | Filter resonance (0.1 to 10.0) |

### ReverbNode (effect)

Wraps Godot's AudioEffectReverb.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `room_size` | float | 0.8 | Room size (0.0 to 1.0) |
| `damping` | float | 0.5 | High-frequency damping (0.0 to 1.0) |
| `wet` | float | 0.3 | Wet signal level |
| `dry` | float | 0.7 | Dry signal level |

### GainNode (effect)

Volume control. Wraps AudioEffectAmplify and also sets the bus volume.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `gain` | float | 1.0 | Linear gain multiplier (0.0 to 2.0) |

### OutputNode (terminal)

Routes the audio chain to a named AudioServer bus.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bus_name` | String | "Master" | Target AudioServer bus |

## Usage

### Visual Editor

1. Open the "AudioGraph" dock.
2. Add nodes (Oscillator, Filter, Reverb, Gain, Output) from the graph editor toolbar.
3. Connect nodes by dragging between ports.
4. Press Play to execute the graph. Audio will be produced in real time.
5. Press Stop to tear down the audio chain and clean up buses.

### Script

```gdscript
var graph := AudioGraph.new()

var osc_idx := graph.add_node(OscillatorNode.new())
var filter_idx := graph.add_node(FilterNode.new())
var gain_idx := graph.add_node(GainNode.new())
var output_idx := graph.add_node(OutputNode.new())

# Oscillator -> Filter -> Gain -> Output
graph.connect_nodes(osc_idx, 0, filter_idx, 0)
graph.connect_nodes(filter_idx, 0, gain_idx, 0)
graph.connect_nodes(gain_idx, 0, output_idx, 0)

# Configure
(graph.nodes[osc_idx] as OscillatorNode).frequency = 220.0
(graph.nodes[osc_idx] as OscillatorNode).waveform = OscillatorNode.Waveform.SAW
(graph.nodes[filter_idx] as FilterNode).cutoff_frequency = 800.0
(graph.nodes[gain_idx] as GainNode).gain = 0.7

# Play (requires scene tree for AudioStreamPlayer creation)
graph.execute(get_tree())

# Call every frame to keep the oscillator buffer filled:
# graph.pump_audio()

# Stop and clean up:
# graph.stop()
```

In `_process()`, call `graph.pump_audio()` to continuously fill the oscillator's AudioStreamGenerator buffer and prevent audio dropouts.

## Architecture

When `execute()` is called, the AudioGraph:

1. Performs a topological sort from the OutputNode backward to find all reachable nodes.
2. For each OscillatorNode (source), creates a dedicated AudioServer bus.
3. Adds AudioEffect instances (Filter, Reverb, Amplify) to the bus in topological order.
4. Sets bus send to the OutputNode's target bus (default "Master").
5. Creates an AudioStreamPlayer with an AudioStreamGenerator, routes it to the bus, and fills the initial buffer with waveform samples.

`pump_audio()` refills generator buffers each frame for continuous playback.

`stop()` tears everything down: stops players, removes created buses, frees nodes.

## Current Limitations

- **Built on Godot AudioServer.** All audio processing uses Godot's built-in audio effects and bus system. Sample-accurate DSP or custom audio processing is not possible without native code.
- **Single chain per source.** Each oscillator routes through one linear chain of effects to the output. Parallel effect paths, mixing, and splitting are not supported.
- **No audio file input.** Only oscillator-generated tones are supported. AudioStreamPlayer/WAV/OGG input is not implemented.
- **No modulation.** Parameters are static. LFO modulation, envelopes, and parameter automation are not implemented.
- **No MIDI.** No MIDI input or keyboard triggering.
- **Future LabSound integration planned.** The current GDScript implementation is a prototype. A native C++ backend using LabSound is planned to provide sample-accurate processing, more effect types, and lower latency.
- **Buses are created at runtime.** AudioServer buses are added dynamically, which can cause brief audio glitches on first play.
