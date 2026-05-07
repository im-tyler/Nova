#!/usr/bin/env bash
# Setup verifier for the Godot-Unreal Parity Demo Project.
# Confirms addons are present and GDExtension binaries are built.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
UMBRELLA_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

echo "Setting up Godot-Unreal Parity Demo at: $PROJECT_DIR"

# Verify addons
mkdir -p "$PROJECT_DIR/addons"

required_addons=(scatter resonance kinetic)
echo ""
echo "Addon check:"
for addon in "${required_addons[@]}"; do
    if [ -d "$PROJECT_DIR/addons/$addon" ] && [ -f "$PROJECT_DIR/addons/$addon/plugin.cfg" ]; then
        echo "  [OK] addons/$addon"
    else
        echo "  [MISSING] addons/$addon — copy from $UMBRELLA_ROOT/$addon/$addon-plugin/"
    fi
done

# Verify binaries
echo ""
echo "Binary check (rebuild from cascade/cascade and tempest/tempest if missing):"
for lib in libcascade.macos.template_debug.universal.dylib libtempest.macos.template_debug.universal.dylib; do
    if [ -f "$PROJECT_DIR/bin/$lib" ]; then
        echo "  [OK] bin/$lib"
    else
        echo "  [MISSING] bin/$lib"
    fi
done

echo ""
echo "Setup complete. Open with: godot --path $PROJECT_DIR"
