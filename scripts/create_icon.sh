#!/usr/bin/env bash
# Generates icons/AppIcon.icns from icons/AppIcon.svg.
# Requires: rsvg-convert (librsvg), iconutil (macOS built-in)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
SVG="$ROOT/icons/AppIcon.svg"
ICONSET="$ROOT/icons/AppIcon.iconset"
ICNS="$ROOT/icons/AppIcon.icns"

if ! command -v rsvg-convert &>/dev/null; then
    echo "Error: rsvg-convert not found. Install with: brew install librsvg" >&2
    exit 1
fi

rm -rf "$ICONSET"
mkdir -p "$ICONSET"

render() {
    local size=$1 name=$2
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$ICONSET/$name"
}

render 16   icon_16x16.png
render 32   icon_16x16@2x.png
render 32   icon_32x32.png
render 64   icon_32x32@2x.png
render 128  icon_128x128.png
render 256  icon_128x128@2x.png
render 256  icon_256x256.png
render 512  icon_256x256@2x.png
render 512  icon_512x512.png
render 1024 icon_512x512@2x.png

iconutil -c icns "$ICONSET" -o "$ICNS"
rm -rf "$ICONSET"

echo "Created: $ICNS"
