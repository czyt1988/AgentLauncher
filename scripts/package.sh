#!/usr/bin/env bash
# AgentLauncher Windows packaging script.
# Usage: bash scripts/package.sh  (or double-click in Explorer)
#
# Prerequisites:
#   - Qt 6.7.3 (msvc2019_64) installed at the path below
#   - Visual Studio 2019 installed at the path below
#
# To customize for your environment, edit the CONFIG section below.

set -euo pipefail

# ============================================================================
#  CONFIG — edit these paths to match your installation
# ============================================================================

# Qt installation prefix (must point to e.g. <Qt>/<version>/<compiler>)
QT_PREFIX="D:/Qt/6.7.3/msvc2019_64"

# Visual Studio vcvars64.bat path (used to set up MSVC env when not already
# loaded, e.g. when double-clicking the script). Leave empty to skip.
VCVARS="D:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars64.bat"

# Build output and distribution directories
BUILD_DIR="build-release"
DIST_DIR="dist/AgentLauncher"

# Version number (used in the zip file name)
VERSION="1.0.0"

# ============================================================================
#  Main script — no need to edit below this line
# ============================================================================

# Keep the window open when double-clicked (works for both success and error)
trap 'echo ""; read -rp "Press Enter to close..."' EXIT

# Always run from the project root, regardless of where the script is invoked
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

ZIP_NAME="AgentLauncher-${VERSION}-win64.zip"

# Auto-load MSVC environment if dumpbin isn't on PATH (needed by windeployqt)
if ! command -v dumpbin &>/dev/null; then
    if [[ -n "$VCVARS" && -f "$VCVARS" ]]; then
        echo "Loading MSVC environment..."
        eval "$(cmd //c "\"$VCVARS\" >nul 2>nul && set" 2>/dev/null | sed -E 's/^([A-Za-z0-9_]+)=(.*)$/export \1="\2"/')"
    fi
fi

echo "=== [1/4] Release build ==="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD_DIR" --config Release

echo ""
echo "=== [2/4] Prepare dist directory ==="
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
# MSBuild (VS generator) puts Release output under $BUILD_DIR/Release/;
# Ninja puts it directly in $BUILD_DIR/. Handle both.
if [[ -f "$BUILD_DIR/Release/AgentLauncher.exe" ]]; then
    cp "$BUILD_DIR/Release/AgentLauncher.exe" "$DIST_DIR/"
else
    cp "$BUILD_DIR/AgentLauncher.exe" "$DIST_DIR/"
fi

echo ""
echo "=== [3/4] windeployqt: pull Qt/QML dependencies ==="
"$QT_PREFIX/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler \
    --qmldir qml "$DIST_DIR/AgentLauncher.exe"

echo ""
echo "=== [4/4] Package as zip ==="
# Use 7-Zip if available, otherwise fall back to PowerShell
if command -v 7z &>/dev/null; then
    7z a -tzip "dist/$ZIP_NAME" "$DIST_DIR"
else
    powershell -NoProfile -Command \
        "Compress-Archive -Path '$DIST_DIR' -DestinationPath 'dist/$ZIP_NAME' -Force"
fi

echo ""
echo "=== Done ==="
echo "Distribution directory: $DIST_DIR"
echo "Zip archive:            dist/$ZIP_NAME"
