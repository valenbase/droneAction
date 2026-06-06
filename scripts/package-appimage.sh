#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
DIST_DIR="${DIST_DIR:-$PROJECT_ROOT/dist}"
APPDIR="${APPDIR:-$PROJECT_ROOT/AppDir}"
VERSION="${VERSION:-$(git -C "$PROJECT_ROOT" describe --tags --always --dirty 2>/dev/null || echo 0.1.0)}"
APPIMAGE_NAME="DroneAction-${VERSION}-x86_64.AppImage"

mkdir -p "$DIST_DIR"
rm -rf "$APPDIR"

cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

TOOLS_DIR="$PROJECT_ROOT/.appimage-tools"
mkdir -p "$TOOLS_DIR"

download_tool() {
    local url="$1"
    local output="$2"
    if [[ ! -x "$TOOLS_DIR/$output" ]]; then
        curl -fsSL "$url" -o "$TOOLS_DIR/$output"
        chmod +x "$TOOLS_DIR/$output"
    fi
}

download_tool "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
    "linuxdeploy-x86_64.AppImage"
download_tool "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
    "linuxdeploy-plugin-qt-x86_64.AppImage"
download_tool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
    "appimagetool-x86_64.AppImage"

export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$TOOLS_DIR:$PATH"

"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/droneaction" \
    --desktop-file "$APPDIR/usr/share/applications/com.valenbase.DroneAction.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/com.valenbase.DroneAction.svg" \
    --plugin qt

ARCH=x86_64 "$TOOLS_DIR/appimagetool-x86_64.AppImage" \
    "$APPDIR" \
    "$DIST_DIR/$APPIMAGE_NAME"

echo "$DIST_DIR/$APPIMAGE_NAME"
