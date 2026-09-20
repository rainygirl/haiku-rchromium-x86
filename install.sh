#!/bin/sh
# One-shot installer for R Chromium (x86) on Haiku.
#
# Run this from a checkout of this repository, on the Haiku machine:
#
#     sh install.sh [BUILD_DIR] [APP_DIR]
#
# BUILD_DIR  directory that holds the built content_shell and its resources
#            (content_shell, content_shell.pak, icudtl.dat, locales/ ...).
#            Default: /boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
# APP_DIR    where to install it. Default: /boot/home/RChromium
#
# It provisions the fontconfig file and helper scripts, verifies the V8
# embedded-builtins blob (repairing it from embedded.o if this machine's linker
# corrupted it), copies the browser and its resources into place, and puts the
# "R Chromium" launcher with the blue Chromium icon on the Desktop. Idempotent.
#
# Building content_shell from source is a separate, much longer job; see
# AGENTS.md. This script installs an already-built binary.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native}
APP_DIR=${2:-/boot/home/RChromium}
EMBEDDED_O="$BUILD/obj/v8/v8_snapshot/embedded.o"

fail() { echo "install.sh: $1" >&2; exit 1; }

[ -x "$BUILD/content_shell" ] || fail "no content_shell in $BUILD
  Pass the build directory as the first argument, or build one first (AGENTS.md)."

echo "R Chromium installer"
echo "  build : $BUILD"
echo "  into  : $APP_DIR"

# 1. Helper scripts install_to_desktop.sh calls by absolute path, plus the blob
#    verifier/repairer.
echo "* provisioning helper scripts and fontconfig"
cp -f "$HERE/scripts/verify_embedded_blob.py" "$HERE/scripts/repair_embedded_blob.py" \
      "$HERE/scripts/scan_zero_pages.py" /boot/home/
cp -f "$HERE/assets/rchromium-fonts.conf" /boot/home/rchromium-fonts.conf

# 2. Repair the embedded blob in place if the link corrupted it (this machine's
#    ld 2.17 does, under its low-memory flags). Safe: the blob has no
#    relocations, so a correct link equals embedded.o's .text byte for byte.
if ! python3 /boot/home/verify_embedded_blob.py "$BUILD/content_shell" >/dev/null 2>&1; then
    if [ -f "$EMBEDDED_O" ]; then
        echo "* embedded blob is corrupt -- repairing from embedded.o"
        python3 /boot/home/repair_embedded_blob.py "$BUILD/content_shell" "$EMBEDDED_O"
        python3 /boot/home/verify_embedded_blob.py "$BUILD/content_shell" >/dev/null \
            || fail "blob still corrupt after repair; rebuild with scripts/linkretry-verified.sh (AGENTS.md)"
    else
        fail "embedded blob is corrupt and $EMBEDDED_O is missing to repair it;
  rebuild with scripts/linkretry-verified.sh (AGENTS.md)."
    fi
fi

# 2b. The blob is not the only thing a bad link damages. On 2026-09-20 a link
#     wrote 142 page-aligned zero pages into .text and .rodata; only 59 of them
#     were inside the blob, so the check above would have passed the rest.
#     Nothing can repair those, so refuse the binary outright.
python3 /boot/home/scan_zero_pages.py "$BUILD/content_shell" \
    || fail "the binary has zero pages where code should be -- it is a bad link.
  Rebuild with scripts/linkretry-verified.sh (AGENTS.md)."

# 3. Copy binary + resources, write the Desktop launcher with the icon.
echo "* installing to $APP_DIR and the Desktop"
sh "$HERE/scripts/install_to_desktop.sh" "$BUILD" "$APP_DIR"

echo
echo "Done. Double-click \"R Chromium\" on the Desktop, or run:"
echo "  \"/boot/home/Desktop/R Chromium\" https://ko.wikipedia.org/"
