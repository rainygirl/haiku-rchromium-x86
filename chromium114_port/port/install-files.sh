#!/bin/bash
# Copy everything under files/ into the Chromium tree at the same path.
# These are whole files this port adds rather than edits to existing ones,
# so they are kept as files instead of as patch scripts.
#
#   install-files.sh <files-dir> <chromium-root>
set -e
SRC="${1:?files dir}"
DST="${2:?chromium root}"
cd "$SRC"
find . -type f | while read -r f; do
	rel="${f#./}"
	mkdir -p "$DST/$(dirname "$rel")"
	cp "$f" "$DST/$rel"
	echo "  $rel"
done
