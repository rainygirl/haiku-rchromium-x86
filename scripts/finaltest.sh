#!/bin/sh
# Load a page, inject keystrokes into whatever has focus, screenshot, report.
URL=${1:-https://x.com/i/flow/login}
TAG=${2:-final1}
WAIT=${3:-100}
TEXT=${4:-rainygirl}
LOG=/boot/home/$TAG.log
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
killcs
cd /boot/home/RChromium || exit 1
FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf \
    ./content_shell --ozone-platform=haiku --single-process --disable-gpu \
    --in-process-gpu --disable-gpu-compositing "$URL" > "$LOG" 2>&1 &
sleep "$WAIT"
printf '%s\n' "$TEXT" > /boot/home/keyinject.in
sleep 15
screenshot --silent "/boot/home/$TAG.png" 2>/dev/null
c=0; n=0
while read -r l; do
    case "$l" in
        *"Check failed"*|*"Fatal error"*|*"Received signal"*) c=$((c+1)) ;;
    esac
    case "$l" in *computed_style*) n=$((n+1)) ;; esac
done < "$LOG"
printf 'crashes=%s computed_style_NOTREACHED=%s shot=/boot/home/%s.png\n' "$c" "$n" "$TAG"
killcs
