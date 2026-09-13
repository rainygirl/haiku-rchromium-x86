#!/bin/sh
# Same retry loop as linkretry.sh, but with RCHROMIUM_NO_NKM=1.
#
# Twelve consecutive attempts with --no-keep-memory were killed at the same
# 141,614,983-byte intermediate. That flag makes ld re-read input sections
# from disk instead of holding them, which trades physical pages for page
# cache churn -- and page cache is exactly what this machine is short of.
# Dropping it puts ld's inputs in its own heap, where the 8 GB swap file can
# take them. --gc-sections and -Wl,-O2 stay on, as they do in linkretry.sh.
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
LOG=/boot/home/linkretry-nice.log
MAX=${LINK_RETRY_MAX:-6}

: > "$LOG"

is_elf()
{
    [ -s "$OUT/content_shell" ] || return 1
    magic=$(head -c 4 "$OUT/content_shell" | od -An -c | tr -d ' \n')
    case "$magic" in
        *ELF*) return 0 ;;
        *)     return 1 ;;
    esac
}

i=1
while [ "$i" -le "$MAX" ]; do
    printf '=== attempt %s starting %s\n' "$i" "$(date)" >> "$LOG"
    cd "$OUT" || exit 1
    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \
    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_GC_NO_O2=1 RCHROMIUM_NO_BUILDID=1 RCHROMIUM_LINK_NICE=15 \
        ninja -C . -j1 content_shell >> "$LOG" 2>&1

    if is_elf; then
        printf '=== SUCCESS on attempt %s at %s (%s bytes)\n' \
            "$i" "$(date)" "$(stat -c %s "$OUT/content_shell")" >> "$LOG"
        # Before anything else can truncate it again.
        cp -f "$OUT/content_shell" /boot/home/content_shell.last-good
        printf '=== kept /boot/home/content_shell.last-good\n' >> "$LOG"
        exit 0
    fi

    printf '=== attempt %s failed at %s (%s bytes)\n' \
        "$i" "$(date)" "$(stat -c %s "$OUT/content_shell" 2>/dev/null)" >> "$LOG"
    i=$((i + 1))
    sleep 30
done

printf '=== gave up after %s attempts at %s\n' "$MAX" "$(date)" >> "$LOG"
exit 1
