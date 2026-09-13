#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /absolute/path/to/qtwebengine-chromium" >&2
    exit 64
fi

checkout=$1
chromium="$checkout/chromium"
gn="$checkout/gn/out/gn"

if [ -d /boot/system/develop/lib/x86/pkgconfig ]; then
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig
    export PKG_CONFIG_PATH
fi

if [ ! -f "$chromium/BUILD.gn" ]; then
    echo "Chromium source not found: $chromium" >&2
    exit 66
fi

if [ ! -x "$gn" ]; then
    if [ -x "$checkout/gn/out/Release/gn" ]; then
        gn="$checkout/gn/out/Release/gn"
    else
        (cd "$checkout/gn" && python3 build/gen.py --no-last-commit-position \
            --cc gcc-x86 --cxx g++-x86 --ld g++-x86 --ar ar-x86 && \
            ninja -C out gn)
    fi
fi

# Do not put a platform name in this Chromium 87 output directory.
out="$chromium/out/rchromium_headless"
args='target_os="haiku"
target_cpu="x86"
is_debug=false
is_component_build=false
rchromium_target="//content/shell:content_shell"
use_qt=false
use_custom_libcxx=false
use_gold=false
enable_swiftshader=false
use_ozone=true
use_x11=false
ozone_auto_platforms=false
ozone_platform="headless"
ozone_platform_headless=true
use_sysroot=false
use_glib=false
use_gio=false
use_dbus=false
use_udev=false
use_cups=false
use_pulseaudio=false
use_alsa=false
use_kerberos=false
toolkit_views=false
use_jumbo_build=true
symbol_level=0
blink_symbol_level=0
v8_symbol_level=0
enable_nacl=false
enable_web_speech=false
enable_print_preview=false
enable_remoting=false
enable_widevine=false
proprietary_codecs=true
ffmpeg_branding="Chrome"'

mkdir -p "$out"
printf '%s\n' "$args" > "$out/args.gn"
"$gn" gen "$out"
ninja -C "$out" -j1 content_shell
