#!/bin/sh
set -eu

expected_upstream=4d8433e345aa23bfb55be66f0aa13656ee92fa38
haikuports_revision=3fb92032fd276268f38665ddfff1eddc670419fa
patch_sha256=8ddfb1575779a8cd3162eb5a571764d414a01b1a042ae16f4d96e8a6177803bb

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /absolute/path/to/qtwebengine-chromium" >&2
    exit 64
fi

checkout=$1
case "$checkout" in
    /*) ;;
    *) echo "checkout path must be absolute" >&2; exit 64 ;;
esac

if [ ! -d "$checkout/.git" ] || [ ! -f "$checkout/chromium/BUILD.gn" ]; then
    echo "not a qtwebengine-chromium checkout: $checkout" >&2
    exit 66
fi

if git -C "$checkout" log -20 --format=%s | grep -q '^Patchset based on qtwebengine patches'; then
    echo "HaikuPorts Chromium patchset is already applied"
    exit 0
fi

actual=$(git -C "$checkout" rev-parse HEAD)
if [ "$actual" != "$expected_upstream" ]; then
    echo "unexpected Chromium base: $actual" >&2
    echo "expected: $expected_upstream" >&2
    exit 65
fi
if ! git -C "$checkout" diff --quiet || ! git -C "$checkout" diff --cached --quiet; then
    echo "checkout has local changes; refusing to apply patchset" >&2
    exit 73
fi

patch_file="${TMPDIR:-/tmp}/qtwebengine-chromium-5.15.18.patchset"
patch_url="https://raw.githubusercontent.com/haikuports/haikuports/$haikuports_revision/dev-qt/qtwebengine/patches/qtwebengine-chromium-5.15.18.patchset"
curl -L --fail --output "$patch_file" "$patch_url"

actual_patch_sha=$(sha256sum "$patch_file" | cut -d ' ' -f 1)
if [ "$actual_patch_sha" != "$patch_sha256" ]; then
    echo "patch checksum mismatch: $actual_patch_sha" >&2
    exit 74
fi

git -C "$checkout" am "$patch_file"
echo "Applied pinned Haiku patchset to $checkout"
