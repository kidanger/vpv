#!/usr/bin/env bash
# Usage:
#   ./package/make_app_bundle.sh <path-to-vpv.app>
#
# Requirements (macOS):
#   brew install dylibbundler

set -euo pipefail

APP="${1:?Usage: $0 <path-to-vpv.app>}"
MACOS="$APP/Contents/MacOS"
LIBS="$APP/Contents/libs"

# ── Bundle dylibs ────────────────────────────────────────────────────────────
echo "==> Bundling dylibs with dylibbundler"
dylibbundler \
    --bundle-deps \
    --fix-file "$MACOS/vpv" \
    --dest-dir "$LIBS" \
    --install-path "@executable_path/../libs/" \
    --overwrite-dir

# ── Ad-hoc code sign ─────────────────────────────────────────────────────────
# This removes the "app is damaged" Gatekeeper error on Apple Silicon / macOS 13+
# without requiring a paid Apple Developer certificate.
if command -v codesign &>/dev/null; then
    echo "==> Ad-hoc signing"
    codesign --deep --force --sign - "$APP"
fi

echo "==> Done: $APP"
