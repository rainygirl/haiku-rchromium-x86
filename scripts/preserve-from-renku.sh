#!/bin/sh
# Pull finished build output off the Haiku box onto the Mac.
#
# Why this exists: on 2026-09-01 the VAIO lost two full days of build output in
# one day. After a hard halt in the morning and a link-stage kernel panic that
# evening, every file under out/rchromium_native came back dated 09:03 and
# files copied in at 18:00 were simply gone. checkfs -c found the volume
# structurally clean, so the disk and bfs are fine -- the kernel's page writer
# stops making progress under sustained build I/O, dirty pages pile up in RAM,
# and a crash takes all of it. The watchdog now syncs every pass to bound that,
# but anything worth keeping should not live only on that machine.
#
# Haiku cannot push here (the Mac has Remote Login off), so this pulls.
#
# Usage:
#   scripts/preserve-from-renku.sh            # pull if content_shell exists
#   scripts/preserve-from-renku.sh --wait     # poll until it exists, then pull
#
# Each run drops a timestamped directory under preserved/ and updates the
# "latest" symlink. Existing snapshots are never overwritten.

set -eu

HOST=${RENKU_HOST:-user@renku}
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
DEST_ROOT=${PRESERVE_DIR:-$(cd "$(dirname "$0")/.." && pwd)/preserved}

remote() {
    ssh -o ConnectTimeout=20 "$HOST" "$@"
}

binary_ready() {
    remote "test -s $OUT/content_shell" 2>/dev/null
}

if [ "${1:-}" = "--wait" ]; then
    echo "waiting for $OUT/content_shell to appear..."
    until binary_ready; do
        sleep 120
    done
fi

if ! binary_ready; then
    echo "content_shell not present (or empty) on $HOST; nothing to preserve" >&2
    exit 1
fi

# Flush first: the binary may exist while its tail is still only in the page
# cache, which is exactly how this machine loses things.
remote 'sync' || true

stamp=$(date +%Y%m%d-%H%M%S)
dest="$DEST_ROOT/$stamp"
mkdir -p "$dest"

echo "preserving to $dest"

# content_shell is the point of the exercise; the rest is what it needs to
# actually run, plus the build state that makes an incremental rebuild possible
# instead of starting over.
for f in \
    content_shell \
    content_shell.pak \
    icudtl.dat \
    v8_context_snapshot.bin \
    snapshot_blob.bin \
    args.gn \
    .ninja_log
do
    if remote "test -e $OUT/$f" 2>/dev/null; then
        echo "  $f"
        scp -q -o ConnectTimeout=20 "$HOST:$OUT/$f" "$dest/" || echo "  (failed: $f)" >&2
    fi
done

if [ ! -s "$dest/content_shell" ]; then
    echo "content_shell did not copy; leaving $dest for inspection" >&2
    exit 1
fi

( cd "$DEST_ROOT" && rm -f latest && ln -s "$stamp" latest )

echo
ls -la "$dest"
echo
echo "latest -> $DEST_ROOT/latest"
