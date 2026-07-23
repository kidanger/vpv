#!/usr/bin/env bash
# Package vpv.app into a distributable .dmg disk image.
#
# Usage:
#   ./package/make_dmg.sh <path-to-vpv.app> [output-dir]
#
# Output: <output-dir>/vpv.dmg  (default: current directory)
#
# Requirements: macOS only (uses hdiutil, which ships with macOS)

set -euo pipefail

APP="${1:?Usage: $0 <path-to-vpv.app> [output-dir]}"
OUTDIR="${2:-.}"
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
OUTDIR="$(mkdir -p "$OUTDIR" && cd "$OUTDIR" && pwd)"

FINAL_DMG="$OUTDIR/vpv.dmg"
STAGING="$(mktemp -d)/vpv-dmg-staging"
TMP_DMG="$(mktemp -d)/vpv-tmp.dmg"

echo "==> Creating DMG from $APP"

mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

# Compute a size with some headroom (in MB)
APP_SIZE_KB=$(du -sk "$APP" | cut -f1)
DMG_SIZE_MB=$(( (APP_SIZE_KB / 1024) + 16 ))

echo "==> Building temporary writable DMG (~${DMG_SIZE_MB} MB)"
hdiutil create \
    -srcfolder "$STAGING" \
    -volname "vpv" \
    -fs HFS+ \
    -fsargs "-c c=64,a=16,b=16" \
    -format UDRW \
    -size "${DMG_SIZE_MB}m" \
    "$TMP_DMG"

# Convert to compressed read-only DMG
echo "==> Compressing to final DMG: $FINAL_DMG"
rm -f "$FINAL_DMG"
hdiutil convert "$TMP_DMG" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$FINAL_DMG"

rm -rf "$STAGING" "$(dirname "$TMP_DMG")"

echo "==> Done: $FINAL_DMG"
