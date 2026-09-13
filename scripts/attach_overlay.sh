#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/chromium/src" >&2
    exit 64
fi

chromium_src=$1
case "$chromium_src" in
    /*) ;;
    *) echo "Chromium src path must be absolute" >&2; exit 64 ;;
esac

if [ ! -f "$chromium_src/DEPS" ] || [ ! -d "$chromium_src/ui/ozone" ]; then
    echo "Not a Chromium src checkout: $chromium_src" >&2
    exit 66
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
overlay="$script_dir/../chromium_overlay"
link="$chromium_src/haiku_port"

if [ -L "$link" ]; then
    current=$(readlink "$link")
    if [ "$current" = "$overlay" ]; then
        echo "Overlay already attached: $link"
        exit 0
    fi
    echo "Refusing to replace existing link: $link -> $current" >&2
    exit 73
fi
if [ -e "$link" ]; then
    echo "Refusing to replace existing path: $link" >&2
    exit 73
fi

ln -s "$overlay" "$link"
echo "Attached $link -> $overlay"
echo "GN arg: ozone_extra_path=\"//haiku_port/ozone_extra.gni\""
