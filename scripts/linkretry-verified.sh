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
    # --gc-sections is not optional on this build, whatever it costs ld.
    #
    # This ladder used to drop it from attempt 3 on. On 2026-09-22 all four
    # attempts that did so failed the same way, and not for memory:
    #
    #   metrics_jumbo_2.o: undefined reference to
    #     `metrics::DriveMetricsProvider::HasSeekPenalty(base::FilePath const&, bool*)'
    #
    # components/metrics only defines HasSeekPenalty in its per-OS files
    # (linux/win/mac/...), none of which this port compiles, so the reference
    # from GetDriveMetricsOnBackgroundThread has always been unresolved --
    # --gc-sections was discarding the section that made it before ld ever had
    # to resolve it. The wrapper's own comments record the same thing happening
    # to protozero_plugin and `wait4'. So dropping the flag cannot produce a
    # binary here; it only exchanges a memory kill for a link error, and the
    # earlier note in this file that "dropping them is what produced a finished
    # binary" was wrong.
    #
    # What is left to trade is -Wl,-O2, which is a string tail-merge pass: it
    # costs a whole-program view for a small size gain and takes nothing away
    # from the output's correctness. Then the build id (ld hashes the finished
    # ~188 MB image to compute it, and nothing in this port reads one). Symbols
    # go last, because scripts/resolve_haiku_stack.py needs .symtab to turn a
    # Haiku backtrace into names -- a stripped binary is worth having only when
    # the alternative is no binary.
    KEEP=1 NOO2= NOBUILDID= STRIP=
    case "$i" in
        1|2) DESC="--gc-sections and -Wl,-O2 kept" ;;
        3|4) NOO2=1; KEEP=; DESC="dropping -Wl,-O2" ;;
        5)   NOO2=1; KEEP=; NOBUILDID=1; DESC="dropping -Wl,-O2 and --build-id" ;;
        *)   NOO2=1; KEEP=; NOBUILDID=1; STRIP=1
             DESC="dropping -Wl,-O2 and --build-id, stripping" ;;
    esac
    printf '=== attempt %s: %s\n' "$i" "$DESC" >> "$LOG"
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \
    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_KEEP_GC=$KEEP RCHROMIUM_GC_NO_O2=$NOO2 \
    RCHROMIUM_NO_BUILDID=$NOBUILDID RCHROMIUM_STRIP=$STRIP \
    RCHROMIUM_LINK_NICE=10 \
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
