#!/bin/sh
# Link content_shell on the VAIO and only keep a binary whose V8 embedded blob
# is byte-correct.
#
# This machine's ld 2.17 needs --no-keep-memory and --reduce-memory-overheads to
# fit a 188 MB link in 2 GB at all (either flag alone or none is killed by the
# low-resource manager at ~141 MB of output), and with both it overwrites the
# interior of V8's 1 MB embedded-builtins blob with stale bytes from earlier
# inputs. The blob has no relocations, so the linker's only job for it is a
# byte copy -- which means it can be repaired after the fact by copying
# embedded.o's .text back over it (proven: a clean link matches embedded.o
# byte-for-byte). So: link, repair, verify, keep.
#
# The blob is not the only thing that comes out wrong. A link run outside this
# script on 2026-09-20 wrote 142 page-aligned zero pages into .text and
# .rodata, 59 of them over the blob, and x.com then crashed reading zeros out
# of Builtins_JSEntry. scan_zero_pages.py is the second gate, and it covers the
# damage the blob comparison cannot see.
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
LOG=/boot/home/linkretry-verified.log
: > "$LOG"
for i in 1 2 3 4 5 6; do
    printf '=== attempt %s starting %s\n' "$i" "$(date)" >> "$LOG"
    cd "$OUT" || exit 1
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \
    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_KEEP_GC=1 \
        ninja -C . -j1 content_shell >> "$LOG" 2>&1
    if [ -f "$OUT/content_shell" ] && head -c 4 "$OUT/content_shell" | grep -q ELF; then
        if ! python3 /boot/home/verify_embedded_blob.py "$OUT/content_shell" >> "$LOG" 2>&1; then
            printf '=== attempt %s: blob corrupt, repairing from embedded.o\n' "$i" >> "$LOG"
            python3 /boot/home/repair_embedded_blob.py "$OUT/content_shell" >> "$LOG" 2>&1
        fi
        if python3 /boot/home/verify_embedded_blob.py "$OUT/content_shell" >> "$LOG" 2>&1 \
           && python3 /boot/home/scan_zero_pages.py "$OUT/content_shell" >> "$LOG" 2>&1; then
            printf '=== SUCCESS (blob + zero-page verified) on attempt %s at %s (%s bytes)\n' "$i" "$(date)" "$(stat -c %s "$OUT/content_shell")" >> "$LOG"
            cp "$OUT/content_shell" /boot/home/content_shell.last-good && printf '=== kept /boot/home/content_shell.last-good\n' >> "$LOG"
            exit 0
        fi
        printf '=== attempt %s: still corrupt after repair -- discarding\n' "$i" >> "$LOG"
        rm -f "$OUT/content_shell"
    else
        printf '=== attempt %s failed at %s\n' "$i" "$(date)" >> "$LOG"
    fi
    sleep 30
done
printf '=== gave up after 6 attempts at %s\n' "$(date)" >> "$LOG"
exit 1
