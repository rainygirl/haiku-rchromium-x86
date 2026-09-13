import io

p = "/boot/home/rchromium-native/scripts/attach_chromium87_overlay.sh"
s = io.open(p, encoding="utf-8").read()

if "missing_sources" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

anchor = "patch_state=\"$checkout/.rchromium-native-patches\""
assert s.count(anchor) == 1, "patch_state anchor"

block = '''# The distribution also drops individual files that its own BUILD.gn files
# still list. They are vendored unmodified from the pinned Chromium revision
# and linked into place; see upstream/README.md for provenance.
missing_sources="$overlay/upstream/missing_sources"
if [ -d "$missing_sources" ]; then
    (cd "$missing_sources" && find . -type f) | sed 's|^\\./||' | while read -r rel; do
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

''' + anchor

s = s.replace(anchor, block, 1)
io.open(p, "w", encoding="utf-8").write(s)
print("PATCH_ATTACH2_OK")
