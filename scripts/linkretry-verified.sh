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
#
# RCHROMIUM_LINK_NICE: what kills ld here is the low-resource manager firing
# when free pages go critical, not ld's own peak -- the wrapper's comments say
# so and this run proved it again, five attempts killed with signal 21 on a
# machine with 1.5 GB free. ld at full speed dirties pages faster than the page
# writer flushes them. Running it at a lower priority does not lower the peak;
# it gives the writer room. Without this the link only went through right after
# a reboot.
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
LOG=/boot/home/linkretry-verified.log
: > "$LOG"
for i in 1 2 3 4 5 6; do
    printf '=== attempt %s starting %s\n' "$i" "$(date)" >> "$LOG"
    cd "$OUT" || exit 1
    # Attempts 1 and 2 keep --gc-sections and -Wl,-O2, which give the smaller
    # binary. They are also what makes ld's peak too big for this machine once
    # it has been up a while: six consecutive attempts were killed at 09:48 on
    # 2026-09-21 with nice(1) already in play. From attempt 3 the wrapper
    # strips both -- its own comments record that dropping them is what
    # produced a finished binary -- rather than waiting for a reboot.
    if [ "$i" -le 2 ]; then
        KEEP=1
    else
        KEEP=
        printf '=== attempt %s: dropping --gc-sections and -Wl,-O2\n' "$i" >> "$LOG"
    fi
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \
    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_KEEP_GC=$KEEP RCHROMIUM_LINK_NICE=10 \
        ninja -C . -j1 content_shell >> "$LOG" 2>&1
    rc=$?
    # ninja's exit status matters. Without this the script looked at the
    # binary from the *previous* run, found it valid, and reported SUCCESS
    # after a compile error -- which on 2026-09-22 meant an hour of measuring
    # a stale binary and believing the source had been rebuilt.
    if [ "$rc" != 0 ]; then
        printf '=== attempt %s: ninja exited %s\n' "$i" "$rc" >> "$LOG"
    elif [ -f "$OUT/content_shell" ] && head -c 4 "$OUT/content_shell" | grep -q ELF; then
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
