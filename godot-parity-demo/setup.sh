#!/usr/bin/env bash
# Setup script for Godot-Unreal Parity Demo Project
# Creates symlinks and verifies binary presence.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Setting up Godot-Unreal Parity Demo at: $PROJECT_DIR"

# Symlink GDScript plugins
mkdir -p "$PROJECT_DIR/addons"
ln -sf /Users/tyler/Documents/procgen/scatter-plugin "$PROJECT_DIR/addons/scatter"
ln -sf /Users/tyler/Documents/audio/resonance-plugin "$PROJECT_DIR/addons/resonance"
ln -sf /Users/tyler/Documents/animation/kinetic-plugin/addons/kinetic "$PROJECT_DIR/addons/kinetic"

echo "Symlinks created:"
ls -la "$PROJECT_DIR/addons/"

# Verify binaries
echo ""
echo "Binary check:"
for lib in libcascade.macos.template_debug.universal.dylib libtempest.macos.template_debug.universal.dylib; do
    if [ -f "$PROJECT_DIR/bin/$lib" ]; then
        echo "  [OK] bin/$lib"
    else
        echo "  [MISSING] bin/$lib"
    fi
done

echo ""
echo "Setup complete. Open with: godot --path $PROJECT_DIR"
