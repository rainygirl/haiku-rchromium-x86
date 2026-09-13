#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /absolute/path/to/qtwebengine-chromium" >&2
    exit 64
fi

checkout=$1
chromium="$checkout/chromium"
if [ ! -f "$chromium/DEPS" ] || [ ! -d "$chromium/ui/ozone" ]; then
    echo "not a qtwebengine-chromium checkout: $checkout" >&2
    exit 66
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
overlay=$(CDPATH= cd -- "$script_dir/../chromium87_overlay" && pwd)
link="$chromium/haiku_port"
content_shell_upstream="$overlay/upstream/content_shell"

if [ -L "$link" ] && [ "$(readlink "$link")" = "$overlay" ]; then
    echo "Chromium 87 Haiku overlay already attached"
else
    if [ -e "$link" ] || [ -L "$link" ]; then
        echo "refusing to replace existing path: $link" >&2
        exit 73
    fi
    ln -s "$overlay" "$link"
fi

# QtWebEngine's source distribution omits Content Shell implementation
# directories even though content/shell/BUILD.gn still references them. Keep
# the exact Chromium 87.0.4280.144 sources in this overlay and attach them
# without copying or editing the pinned checkout.
for directory in app browser gpu renderer utility; do
    source_directory="$content_shell_upstream/$directory"
    shell_link="$chromium/content/shell/$directory"
    if [ ! -d "$source_directory" ]; then
        echo "missing pinned Content Shell source: $source_directory" >&2
        exit 66
    fi
    if [ -L "$shell_link" ] && [ "$(readlink "$shell_link")" = "$source_directory" ]; then
        continue
    fi
    if [ -e "$shell_link" ] || [ -L "$shell_link" ]; then
        echo "refusing to replace existing path: $shell_link" >&2
        exit 73
    fi
    ln -s "$source_directory" "$shell_link"
done

# The distribution also drops individual files that its own BUILD.gn files
# still list. They are vendored unmodified from the pinned Chromium revision
# and linked into place; see upstream/README.md for provenance.
missing_sources="$overlay/upstream/missing_sources"
if [ -d "$missing_sources" ]; then
    (cd "$missing_sources" && find . -type f) | sed 's|^\./||' | while read -r rel; do
        target="$chromium/$rel"
        source_file="$missing_sources/$rel"
        if [ -L "$target" ] && [ "$(readlink "$target")" = "$source_file" ]; then
            continue
        fi
        if [ -e "$target" ] || [ -L "$target" ]; then
            echo "refusing to replace existing path: $target" >&2
            exit 73
        fi
        mkdir -p "$(dirname "$target")"
        ln -s "$source_file" "$target"
        echo "Linked missing upstream source: $rel"
    done
fi

patch_state="$checkout/.rchromium-native-patches"
mkdir -p "$patch_state"
for patch in "$overlay"/patches/*.patch; do
    [ -f "$patch" ] || continue
    patch_name=$(basename "$patch")
    marker="$patch_state/$patch_name"
    patch_hash=$(sha256sum "$patch" | cut -d ' ' -f 1)
    if [ -f "$marker" ]; then
        recorded_hash=$(cat "$marker")
        if [ "$recorded_hash" = "$patch_hash" ]; then
            echo "Supplemental patch already recorded: $patch_name"
            continue
        fi
        echo "recorded patch checksum changed: $patch_name" >&2
        exit 74
    fi
    if git -C "$checkout" apply --check "$patch" 2>/dev/null; then
        git -C "$checkout" apply "$patch"
        printf '%s\n' "$patch_hash" > "$marker"
        echo "Applied supplemental patch: $patch_name"
    elif git -C "$checkout" apply --reverse --check "$patch" 2>/dev/null; then
        printf '%s\n' "$patch_hash" > "$marker"
        echo "Supplemental patch already applied: $patch_name"
    else
        echo "supplemental patch does not apply cleanly: $patch" >&2
        exit 65
    fi
done
# Five Content Shell directories are symlinks into this overlay, so patches
# touching them cannot be applied through the pinned checkout: git refuses to
# write beyond a symbolic link. They are kept as a separate ordered series
# applied against the overlay itself.
upstream_patch_state="$overlay/.rchromium-upstream-patches"
mkdir -p "$upstream_patch_state"
for patch in "$overlay"/patches-upstream/*.patch; do
    [ -f "$patch" ] || continue
    patch_name=$(basename "$patch")
    marker="$upstream_patch_state/$patch_name"
    patch_hash=$(sha256sum "$patch" | cut -d ' ' -f 1)
    if [ -f "$marker" ]; then
        recorded_hash=$(cat "$marker")
        if [ "$recorded_hash" = "$patch_hash" ]; then
            echo "Upstream patch already recorded: $patch_name"
            continue
        fi
        echo "recorded upstream patch checksum changed: $patch_name" >&2
        exit 74
    fi
    if git -C "$overlay" apply --check "$patch" 2>/dev/null; then
        git -C "$overlay" apply "$patch"
        printf '%s\n' "$patch_hash" > "$marker"
        echo "Applied upstream patch: $patch_name"
    elif git -C "$overlay" apply --reverse --check "$patch" 2>/dev/null; then
        printf '%s\n' "$patch_hash" > "$marker"
        echo "Upstream patch already applied: $patch_name"
    else
        echo "upstream patch does not apply cleanly: $patch" >&2
        exit 65
    fi
done
echo "Attached $link -> $overlay"
