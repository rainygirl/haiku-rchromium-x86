#!/bin/sh
# Two flag arms on the same page, alternating, so the machine's own drift
# lands on both. Ten runs each: the baseline came out 5 of 10 and the last
# four of those were consecutive, which is exactly the shape that makes a
# non-interleaved comparison worthless.
#
# The crash is a SIGSEGV at address 0 inside ICU's appendResult(), reached
# from u_strToLower through V8's Runtime_StringToLowerCaseIntl, on the
# renderer thread, under nineteen stacked InterpreterEntryTrampoline frames.
# Patch 0090 already found and fixed one stack overflow with that shape by
# raising Haiku's 256 kB non-main thread stack to 4 MB. If lowering V8's own
# limit well below that stack changes the rate, the fix did not take or 4 MB
# is not what the renderer thread actually gets.
URL=${1:-https://x.com/i/flow/login}
ARM2=${2:---js-flags=--stack-size=256}
RUNS=${3:-10}
WAIT=${4:-70}
OUT=/boot/home/xarm.txt
: > "$OUT"
N=0

killcs() {
    ps | while read -r line; do
        case "$line" in *content_shell*) ;; *) continue ;; esac
        # shellcheck disable=SC2086
        set -- $line
        n=$#; [ "$n" -lt 4 ] && continue
        i=$((n - 3)); eval "id=\${$i}"
        case "$id" in ''|*[!0-9]*) continue ;; esac
        kill -9 "$id" 2>/dev/null
    done
    sleep 3
}

one() {
    N=$((N + 1))
    LOG=/boot/home/xa$N.log
    killcs
    cd /boot/home/RChromium || exit 1
    FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf \
        ./content_shell --ozone-platform=haiku --single-process --disable-gpu \
        --in-process-gpu --disable-gpu-compositing $1 "$URL" > "$LOG" 2>&1 &
    sleep "$WAIT"
    c=0
    while read -r l; do
        case "$l" in
            *"Check failed"*|*"Fatal error"*|*"Received signal"*) c=$((c+1)) ;;
        esac
    done < "$LOG"
    if [ "$c" != 0 ]; then V="X"; else V="."; fi
    killcs
}

A=""
B=""
k=0
while [ "$k" -lt "$RUNS" ]; do
    one "";      A="$A$V"
    one "$ARM2"; B="$B$V"
    k=$((k + 1))
    printf 'baseline   %s\nstack-256  %s\n' "$A" "$B" > "$OUT"
done
printf 'baseline   %s\nstack-256  %s\nX = crashed, . = survived\n---- done ----\n' \
    "$A" "$B" > "$OUT"
