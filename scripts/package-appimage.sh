#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
DIST_DIR="${DIST_DIR:-$PROJECT_ROOT/dist}"
APPDIR="${APPDIR:-$PROJECT_ROOT/AppDir}"
APPIMAGE_NAME="${APPIMAGE_NAME:-droneAction.appimage}"

mkdir -p "$DIST_DIR"
find "$DIST_DIR" -maxdepth 1 -type f \( -name "*.AppImage" -o -name "*.appimage" \) -delete
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

if [[ -z "${QMAKE:-}" ]]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE="$(command -v qmake6)"
    elif [[ -x /usr/lib/qt6/bin/qmake6 ]]; then
        QMAKE="/usr/lib/qt6/bin/qmake6"
    elif [[ -x /usr/lib/x86_64-linux-gnu/qt6/bin/qmake6 ]]; then
        QMAKE="/usr/lib/x86_64-linux-gnu/qt6/bin/qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE="$(command -v qmake)"
    else
        echo "Unable to find qmake/qmake6 for linuxdeploy-plugin-qt" >&2
        exit 1
    fi
fi

export QMAKE
echo "Using QMAKE=$QMAKE"
"$QMAKE" -query QT_VERSION

APPIMAGETOOL_ARGS=()
if [[ -n "${APPIMAGE_RUNTIME_FILE:-}" ]]; then
    APPIMAGETOOL_ARGS+=(--runtime-file "$APPIMAGE_RUNTIME_FILE")
elif [[ -f "$TOOLS_DIR/runtime-x86_64" ]]; then
    APPIMAGETOOL_ARGS+=(--runtime-file "$TOOLS_DIR/runtime-x86_64")
fi

"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/droneaction" \
    --desktop-file "$APPDIR/usr/share/applications/com.valenbase.DroneAction.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/com.valenbase.DroneAction.svg" \
    --plugin qt

ARCH=x86_64 "$TOOLS_DIR/appimagetool-x86_64.AppImage" \
    "${APPIMAGETOOL_ARGS[@]}" \
    "$APPDIR" \
    "$DIST_DIR/$APPIMAGE_NAME"

echo "$DIST_DIR/$APPIMAGE_NAME"
