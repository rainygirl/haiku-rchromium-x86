#!/bin/bash
# Restore source files the official tarball omits but its own BUILD.gn lists.
#
# Chromium's release tarballs drop some test data; content/test/BUILD.gn still
# names six mojom files, and ninja stops before compiling anything with
# "missing and no known rule to make it". The 87 port had the same class of
# problem and vendored the files from the pinned revision -- same idea here,
# fetched from the tag rather than carried in the repo.
#
# Six, not four: the first pass grepped for *.test-mojom and missed two plain
# .mojom ones, so ninja was run again for a second name. `ninja -t inputs
# content_shell` lists all 101,252 inputs and answers the question once --
# 33 absent, of which 31 are test fonts and two are these.
set -e
SRC="${1:-/build/chromium108}"
TAG=108.0.5359.124
BASE="https://chromium.googlesource.com/chromium/src/+/refs/tags/$TAG"
FILES="
content/test/data/web_ui_test.test-mojom
content/test/data/web_ui_test_types.test-mojom
content/test/data/mojo_bindings_web_test.test-mojom
content/test/data/mojo_bindings_web_test_types.test-mojom
content/test/data/lite_js_test.mojom
content/test/data/mojo_web_test_helper_test.mojom
"
for rel in $FILES; do
    out="$SRC/$rel"
    [ -f "$out" ] && { echo "  have $rel"; continue; }
    mkdir -p "$(dirname "$out")"
    # gitiles serves base64 with ?format=TEXT
    curl -s "$BASE/$rel?format=TEXT" | base64 -d > "$out"
    if [ -s "$out" ]; then
        echo "  fetched $rel ($(wc -l < "$out") lines)"
    else
        rm -f "$out"
        echo "  FAILED $rel"
    fi
done

# The other 31 absent inputs are test fonts under third_party/test_fonts,
# which the tarball omits deliberately -- they are only copied next to the
# binary for web tests. Placeholders keep ninja from stopping; nothing reads
# them in a browser run, and a real font here would be 30 MB of download for
# a file this port never opens.
FONTDIR="$SRC/third_party/test_fonts/test_fonts"
if [ -d "$FONTDIR" ] || mkdir -p "$FONTDIR"; then
    made=0
    for f in $(cd "$SRC" && grep -rho "test_fonts/[A-Za-z0-9._-]*\.\(ttf\|ttc\|otf\)" \
               third_party/test_fonts 2>/dev/null | sort -u); do
        out="$SRC/third_party/test_fonts/$f"
        mkdir -p "$(dirname "$out")"
        [ -e "$out" ] || { : > "$out"; made=$((made + 1)); }
    done
    echo "  placeholder fonts: $made"
fi
