#!/usr/bin/env bash
# Builds ImageEditor.app, deploys Qt libraries, and packages a DMG.
# Usage: scripts/create_dmg.sh [--skip-build]
#   --skip-build  Skip the cmake/make step (use existing build_dmg/ output).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
BUILD_DIR="$ROOT/build_dmg"
ICNS="$ROOT/icons/AppIcon.icns"
APP_NAME="ImageEditor"
VERSION="1.0.2"
DMG_NAME="${APP_NAME}-${VERSION}.dmg"
CMAKE="/opt/homebrew/bin/cmake"
MACDEPLOYQT="/opt/homebrew/opt/qtbase/bin/macdeployqt"
CREATE_DMG="/opt/homebrew/bin/create-dmg"

# ── Checks ────────────────────────────────────────────────────────────────────
for tool in "$CMAKE" "$MACDEPLOYQT" "$CREATE_DMG"; do
    if ! command -v "$tool" &>/dev/null && ! [ -x "$tool" ]; then
        echo "Error: required tool not found: $tool" >&2
        exit 1
    fi
done

# ── Icon ──────────────────────────────────────────────────────────────────────
if [ ! -f "$ICNS" ]; then
    echo "→ AppIcon.icns not found, running create_icon.sh …"
    bash "$SCRIPT_DIR/create_icon.sh"
fi

# ── Build ─────────────────────────────────────────────────────────────────────
if [[ "${1:-}" != "--skip-build" ]]; then
    echo "→ Configuring (BUILD_MACOS_BUNDLE=ON) …"
    "$CMAKE" -B "$BUILD_DIR" -S "$ROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_MACOS_BUNDLE=ON
    echo "→ Building …"
    "$CMAKE" --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.logicalcpu)"
fi

APP_PATH="$BUILD_DIR/${APP_NAME}.app"
if [ ! -d "$APP_PATH" ]; then
    echo "Error: $APP_PATH not found after build" >&2
    exit 1
fi

# ── Deploy Qt ─────────────────────────────────────────────────────────────────
# macdeployqt can't deploy libqcocoa.dylib because that plugin's LC_RPATH
# only points to qtbase/lib, but QtPdf/QtVirtualKeyboard/QtVirtualKeyboardQml
# live in other Homebrew formulas.
#
# Fix: after the first macdeployqt pass (which deploys everything else),
# we pre-stage libqcocoa.dylib in the bundle WITH extra rpaths pointing to
# /opt/homebrew/opt/qt/lib (meta-formula that has symlinks to all frameworks).
# A second macdeployqt pass with -executable= then handles the full recursive
# deployment of libqcocoa.dylib's dependencies — including transitive ones.

PLATFORMS="$APP_PATH/Contents/PlugIns/platforms"
LIBQCOCOA_BUNDLE="$PLATFORMS/libqcocoa.dylib"
LIBQCOCOA_SRC="/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib"

echo "→ Running macdeployqt (pass 1) …"
"$MACDEPLOYQT" "$APP_PATH" -always-overwrite 2>&1 || true

if [ ! -f "$LIBQCOCOA_BUNDLE" ]; then
    echo "→ libqcocoa.dylib not deployed — running second pass …"
    mkdir -p "$PLATFORMS"
    cp "$LIBQCOCOA_SRC" "$LIBQCOCOA_BUNDLE"
    chmod u+w "$LIBQCOCOA_BUNDLE"
    # Add rpaths so macdeployqt can resolve QtPdf, QtVirtualKeyboard, etc.
    for extra_rp in \
            /opt/homebrew/opt/qt/lib \
            /opt/homebrew/opt/qtwebengine/lib \
            /opt/homebrew/opt/qtvirtualkeyboard/lib; do
        [ -d "$extra_rp" ] || continue
        otool -l "$LIBQCOCOA_BUNDLE" | grep -qF "path $extra_rp " && continue
        install_name_tool -add_rpath "$extra_rp" "$LIBQCOCOA_BUNDLE"
    done
    codesign --force --sign - "$LIBQCOCOA_BUNDLE"

    echo "→ Running macdeployqt (pass 2 — resolving libqcocoa.dylib deps) …"
    "$MACDEPLOYQT" "$APP_PATH" \
        -executable="$LIBQCOCOA_BUNDLE" 2>&1 || true
fi

if [ ! -f "$LIBQCOCOA_BUNDLE" ]; then
    echo "Error: libqcocoa.dylib still missing after second pass" >&2
    exit 1
fi
echo "✓ libqcocoa.dylib present"

# ── TLS plugins ───────────────────────────────────────────────────────────────
# macdeployqt may miss the TLS backends. Deploy SecureTransport manually —
# it only needs QtNetwork + QtCore (already in Frameworks/) + macOS system libs.
TLS_SRC="/opt/homebrew/opt/qtbase/share/qt/plugins/tls"
TLS_DST="$APP_PATH/Contents/PlugIns/tls"
if [ ! -d "$TLS_DST" ]; then
    echo "→ Deploying TLS plugins …"
    mkdir -p "$TLS_DST"
    for plugin in \
            libqsecuretransportbackend.dylib \
            libqcertonlybackend.dylib; do
        src="$TLS_SRC/$plugin"
        dst="$TLS_DST/$plugin"
        [ -f "$src" ] || continue
        cp "$src" "$dst"
        chmod u+w "$dst"
        while IFS= read -r dep; do
            [[ "$dep" == @rpath/* ]] || continue
            rel="${dep#@rpath/}"
            install_name_tool -change "$dep" \
                "@loader_path/../../Frameworks/$rel" "$dst"
        done < <(otool -L "$src" | awk 'NR>1 {print $1}')
        codesign --force --sign - "$dst"
        echo "  ✓ $plugin"
    done
fi

echo "→ Re-signing bundle …"
codesign --force --deep --sign - "$APP_PATH" || true

# ── DMG ───────────────────────────────────────────────────────────────────────
DMG_OUT="$ROOT/$DMG_NAME"
rm -f "$DMG_OUT"

echo "→ Creating $DMG_NAME …"
"$CREATE_DMG" \
    --volname "$APP_NAME $VERSION" \
    --volicon "$ICNS" \
    --window-pos 200 120 \
    --window-size 660 400 \
    --icon-size 128 \
    --icon "${APP_NAME}.app" 180 170 \
    --hide-extension "${APP_NAME}.app" \
    --app-drop-link 480 170 \
    --no-internet-enable \
    "$DMG_OUT" \
    "$APP_PATH"

echo ""
echo "Done: $DMG_OUT"
