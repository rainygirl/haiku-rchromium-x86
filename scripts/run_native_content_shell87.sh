#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 /absolute/path/to/qtwebengine-chromium [URL]" >&2
    exit 64
fi

checkout=$1
url=${2:-https://www.google.com/}
binary="$checkout/chromium/out/rchromium_native/content_shell"

if [ ! -x "$binary" ]; then
    echo "native Content Shell is not built: $binary" >&2
    exit 66
fi

# Blink aborts at font_cache.cc(472) with "Check failed: false" when fontconfig
# has no configuration to load ("Cannot load default config file: No such
# file: (null)"). Haiku installs no /etc/fonts, so point fontconfig at the
# minimal config that lists the two system font directories. Note the file
# must not contain "--" inside an XML comment, which is not well-formed and
# fails the parse silently enough to look like the same crash.
: "${FONTCONFIG_FILE:=/boot/home/rchromium-fonts.conf}"
export FONTCONFIG_FILE

# The first Haiku compositor is intentionally software-only. This invokes the
# R Chromium Ozone backend directly; no Qt platform plugin is involved.
#
# --single-process is still required: the renderer dies on startup in
# multi-process mode, and viz hands a null AcceleratedWidget across the
# process boundary (HaikuSurfaceFactory falls back to SoleWidget() to cope).
# Both are open items -- drop the flag once a real cross-process widget handle
# is plumbed through.
exec "$binary" \
    --ozone-platform=haiku \
    --single-process \
    --disable-gpu \
    --in-process-gpu \
    --disable-gpu-compositing \
    "$url"
