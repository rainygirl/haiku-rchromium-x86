#!/bin/sh
# What does fontconfig actually see? No rebuild needed for the FC_DEBUG half.
#
# FC_DEBUG bits: 1 MATCH, 16 CACHE, 1024 CONFIG. 1041 asks which config file
# was parsed, which directories it took from it, whether it used a cache or
# scanned, and what each match returned. The bundled fontconfig is a different
# vintage from the system one -- note the cache directory holds a le32d4
# cache-9 written by Haiku's fc-cache 2.17 and no cache-7, which is the format
# Chromium's copy would look for -- so "the system fc-match resolves Noto Sans"
# says nothing about what the browser's copy does.
APPDIR=/boot/home/RChromium
export FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf
export FC_DEBUG=${FC_DEBUG:-1041}
LOG=${2:-/boot/home/fcdbg.log}
cd "$APPDIR" || exit 1
./content_shell --ozone-platform=haiku --single-process --disable-gpu \
    --in-process-gpu --disable-gpu-compositing \
    --enable-logging=stderr --log-level=0 \
    "${1:-file:///boot/home/fonttest.html}" > "$LOG" 2>&1 &
sleep "${3:-45}"
ps | while read -r line; do
    case "$line" in *content_shell*) ;; *) continue ;; esac
    # shellcheck disable=SC2086
    set -- $line
    n=$#; [ "$n" -lt 4 ] && continue
    i=$((n - 3)); eval "id=\${$i}"
    case "$id" in ''|*[!0-9]*) continue ;; esac
    kill -9 "$id" 2>/dev/null
done
echo "log: $LOG ($(wc -l < "$LOG") lines)"
