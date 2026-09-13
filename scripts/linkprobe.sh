#!/bin/sh
# One link attempt with a memory sample every ten seconds.
#
# Eighteen consecutive attempts died at exactly 141,614,983 bytes of output,
# with and without --no-keep-memory, while sysinfo reported 1.7 GB free once
# the loop had stopped. A number that repeats to the byte is not memory
# pressure behaving randomly, so measure what is actually scarce at the moment
# ld is killed instead of guessing at flags again.
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
LOG=/boot/home/linkprobe.log
SAMP=/boot/home/linkprobe-samples.log

: > "$LOG"
: > "$SAMP"

(
    cd "$OUT" || exit 1
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \
    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_KEEP_GC=1 \
        ninja -C . -j1 content_shell >> "$LOG" 2>&1
    echo "LINK-DONE $(date)" >> "$LOG"
) &
LINKPID=$!

while kill -0 "$LINKPID" 2>/dev/null; do
    # Team id is the fourth field from the right in Haiku's ps output; the
    # command column ahead of it contains spaces, so count from the end.
    ldline=$(ps | grep "[/]ld\b" | head -1)
    ldpid=$(echo "$ldline" | awk '{print $(NF-3)}')
    mem=$(sysinfo -mem | head -1)
    outsz=$(stat -c %s "$OUT/content_shell" 2>/dev/null)
    printf '%s out=%s %s' "$(date +%H:%M:%S)" "$outsz" "$mem" >> "$SAMP"
    if [ -n "$ldpid" ]; then
        listarea "$ldpid" 2>/dev/null | awk '
            /^ *[0-9]+ / { n++; vs += $4; rs += $5 }
            END { printf " ld_areas=%d ld_vsize=%dKB ld_alloc=%dKB", n, vs, rs }' >> "$SAMP"
    fi
    printf '\n' >> "$SAMP"
    sleep 10
done

echo "=== final ===" >> "$SAMP"
tail -5 "$LOG" >> "$SAMP"
