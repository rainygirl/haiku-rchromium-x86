#!/bin/sh
# Run content_shell until it takes the intermittent SEGV, keeping the image
# map from the run that actually crashed.
#
# content_shell is ET_DYN and Haiku relocates it somewhere different every
# run -- 0x01e60000 and 0x023b8000 on two consecutive runs -- so a stack log
# is only readable together with the listimage output from its own run. Any
# other pairing produces confident nonsense, which is exactly how the earlier
# fontconfig backtrace was misread.
OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
URL=${1:-http://example.com/}
RUNS=${CRASH_HUNT_RUNS:-8}
LIFE=${CRASH_HUNT_LIFE:-60}
KEEP=/boot/home/crashhunt

mkdir -p "$KEEP"
: > "$KEEP/summary.txt"

n=1
while [ "$n" -le "$RUNS" ]; do
    log="$KEEP/run$n.log"
    img="$KEEP/run$n.images"
    : > "$log"
    : > "$img"

    (
        cd "$OUT" || exit 1
        FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf \
            ./content_shell --ozone-platform=haiku --single-process \
            --disable-gpu --in-process-gpu "$URL" >> "$log" 2>&1
    ) &

    # Capture the image map as soon as the team exists.
    i=0
    while [ "$i" -lt 30 ]; do
        team=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}')
        if [ -n "$team" ]; then
            listimage "$team" > "$img" 2>&1
            [ -s "$img" ] && break
        fi
        i=$((i + 1))
        sleep 1
    done

    # Give it a fixed lifetime, then stop it if it is still up.
    i=0
    while [ "$i" -lt "$LIFE" ]; do
        ps | grep -q "[c]ontent_shell" || break
        i=$((i + 1))
        sleep 1
    done
    team=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}')
    [ -n "$team" ] && kill "$team" 2>/dev/null
    sleep 3

    if grep -q "Received signal" "$log"; then
        printf 'run %s CRASHED: %s\n' "$n" \
            "$(grep 'Received signal' "$log" | head -1)" >> "$KEEP/summary.txt"
        echo "HUNT-DONE crashed on run $n" >> "$KEEP/summary.txt"
        exit 0
    fi
    printf 'run %s survived %ss\n' "$n" "$LIFE" >> "$KEEP/summary.txt"
    n=$((n + 1))
done

echo "HUNT-DONE no crash in $RUNS runs" >> "$KEEP/summary.txt"
