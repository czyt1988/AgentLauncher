#!/usr/bin/env bash
# AgentLauncher Windows packaging script.
# Usage: bash scripts/package.sh
#
# Prerequisites:
#   - Qt 6.7.3 (msvc2019_64) installed
#   - windeployqt on PATH (or set CMAKE_PREFIX_PATH below)
#   - Run from a Developer Command Prompt / after vcvars64.bat
set -euo pipefail

QT_PREFIX="D:/Qt/6.7.3/msvc2019_64"
BUILD_DIR="build-release"
DIST_DIR="dist/AgentLauncher"
VERSION="1.0.0"
ZIP_NAME="AgentLauncher-${VERSION}-win64.zip"

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
windeployqt --release --no-translations --no-system-d3d-compiler \
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
pause
