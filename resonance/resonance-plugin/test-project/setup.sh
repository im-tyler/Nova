#!/bin/bash
# Run this from the test-project directory to set up the plugin symlink.
# This creates a symlink so Godot can find the plugin at res://addons/resonance-plugin/

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

mkdir -p "$SCRIPT_DIR/addons"
ln -sfn "$PLUGIN_DIR" "$SCRIPT_DIR/addons/resonance-plugin"

echo "Symlink created: $SCRIPT_DIR/addons/resonance-plugin -> $PLUGIN_DIR"
echo "Open this project in Godot and enable the Resonance Audio Graph plugin."
