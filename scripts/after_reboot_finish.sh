#!/bin/sh
# Everything that has to happen on a freshly booted machine, in one go.
#
# The content_shell link needs about 1.7 GB while the machine has 2 GB, and
# whether the kernel's low-resource killer fires first depends on how much of
# the page cache is dirty -- which is why a fresh boot links in one to six
# attempts and a machine that has been up sixteen hours has failed twenty-four
# times running. Run this immediately after a reboot, before anything else has
# had time to dirty the cache.
#
# Copy to /boot/home and run with `sh /boot/home/after_reboot_finish.sh`.
set -u

OUT=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
LOG=/boot/home/after-reboot.log

: > "$LOG"
say() { printf '=== %s %s\n' "$(date +%H:%M:%S)" "$*" >> "$LOG"; }

say "uptime: $(uptime)"

# One ninja at a time, always. Two in the same output directory have corrupted
# this build before.
if ps | grep -q "[n]inja"; then
    say "ninja already running -- refusing to start a second one"
    exit 1
fi

say "linking"
sh /boot/home/linkretry-nice.sh
if [ ! -s /boot/home/content_shell.last-good ]; then
    say "link failed; see /boot/home/linkretry-nice.log"
    exit 1
fi
say "linked: $(stat -c %s /boot/home/content_shell.last-good) bytes"

# Qt must not be in there. This is the runtime evidence the port is judged on.
if readelf -d "$OUT/content_shell" 2>/dev/null | grep -qi qt; then
    say "FAILED: Qt appears in NEEDED"
    exit 1
fi
say "no Qt in NEEDED; libbe present: $(readelf -d "$OUT/content_shell" | grep -c libbe)"

say "installing to Desktop"
sh /boot/home/install_to_desktop.sh >> "$LOG" 2>&1

# A/B the toolbar against the painting regression, in one boot, without
# relinking: RCH_NO_TOOLBAR=1 skips AttachBrowserChrome entirely.
for mode in with without; do
    runlog=/boot/home/after-reboot-$mode.log
    : > "$runlog"
    say "run $mode toolbar"
    if [ "$mode" = "without" ]; then
        export RCH_NO_TOOLBAR=1
    else
        unset RCH_NO_TOOLBAR
    fi
    ( cd "$OUT" && FONTCONFIG_FILE=/boot/home/rchromium-fonts.conf \
        ./content_shell --ozone-platform=haiku --single-process \
        --disable-gpu --in-process-gpu http://example.com/ \
        >> "$runlog" 2>&1 ) &
    i=0
    while [ $i -lt 40 ]; do
        grep -q "View::Present" "$runlog" 2>/dev/null && break
        sleep 1
        i=$((i + 1))
    done
    sleep 12
    screenshot --silent /boot/home/after-reboot-$mode.png 2>>"$runlog"
    say "$mode: presents=$(grep -c PresentCanvas "$runlog") signals=$(grep -c 'Received signal' "$runlog")"
    team=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}')
    [ -n "$team" ] && kill "$team"
    sleep 4
done

say "done"
cat "$LOG"
