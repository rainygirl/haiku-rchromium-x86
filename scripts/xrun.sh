#!/bin/sh
# How often does this binary survive x.com, and does typing land when it does?
#
# One run is not a measurement here. The port's SEGV on real pages has been
# intermittent since 2026-09-11, and the file this replaces records a day lost
# to reading four runs as a pattern.
#
# Each run captures listimage while the process is alive, because Chromium's
# in-signal-handler backtrace prints bare addresses and Haiku relocates every
# image differently each time -- without the map from that same run a stack
# trace cannot be attributed to anything.
URL=${1:-https://x.com/i/flow/login}
RUNS=${2:-10}
WAIT=${3:-70}
TEXT=${4:-rainygirl}
OUT=/boot/home/xrun.txt
: > "$OUT"

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

pidof_cs() {
    ps | while read -r line; do
        case "$line" in *content_shell*) ;; *) continue ;; esac
        # shellcheck disable=SC2086
        set -- $line
        n=$#; [ "$n" -lt 4 ] && continue
        i=$((n - 3)); eval "id=\${$i}"
        case "$id" in ''|*[!0-9]*) continue ;; esac
        echo "$id"
        return
    done
}

R=""
k=0
while [ "$k" -lt "$RUNS" ]; do
    k=$((k + 1))
    LOG=/boot/home/xr$k.log
    killcs
    cd /boot/home/RChromium || exit 1
    FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf \
        ./content_shell --ozone-platform=haiku --single-process --disable-gpu \
        --in-process-gpu --disable-gpu-compositing "$URL" > "$LOG" 2>&1 &
    sleep 20
    p=$(pidof_cs | head -1)
    [ -n "$p" ] && listimage "$p" > /boot/home/xr$k.images 2>/dev/null
    sleep $((WAIT - 20))
    printf '%s\n' "$TEXT" > /boot/home/keyinject.in
    sleep 12
    screenshot --silent "/boot/home/xr$k.png" 2>/dev/null
    c=0
    while read -r l; do
        case "$l" in
            *"Check failed"*|*"Fatal error"*|*"Received signal"*) c=$((c+1)) ;;
        esac
    done < "$LOG"
    if [ "$c" != 0 ]; then R="${R}X"; else R="${R}."; fi
    printf '%s\n' "$R" > "$OUT"
    killcs
done
printf '%s\nX = crashed, . = survived\n---- done ----\n' "$R" > "$OUT"
