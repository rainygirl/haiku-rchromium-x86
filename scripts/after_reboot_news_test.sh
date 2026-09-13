#!/bin/sh
# The alternating news-site test, to run immediately after a reboot.
#
# Everything this measures works on a freshly booted machine and stops working
# after a few hours of uptime -- the same dependence the content_shell link
# has. Measured today: at 20 minutes' uptime Google News rendered 2057 frames
# and google.com rendered completely; at 5-8 hours the same binary paints once
# and Blink resolves no styles at all (0 computed_style messages against 2864
# on a run that worked). So run this while the machine is fresh, before
# anything else has had time to run.
set -u

LAUNCH="/boot/home/Desktop/R Chromium"
LOG=/boot/home/newstest.log
OUT=/boot/home/newstest-result.txt

: > "$OUT"
say() { printf '%s\n' "$*" >> "$OUT"; }

say "uptime at start: $(uptime)"

pkill_shell() {
    team=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}')
    [ -n "$team" ] && kill "$team"
    sleep 3
}

pkill_shell
: > "$LOG"

# Navigations go through Shell::LoadURL, the same call the address bar makes.
NAV="35:https://news.google.co.kr/,70:https://news.naver.com/"
NAV="$NAV,105:https://news.google.co.kr/,140:https://news.naver.com/"
NAV="$NAV,175:https://news.google.co.kr/,210:https://news.naver.com/"

RCH_NAV_AFTER="$NAV" "$LAUNCH" https://news.naver.com/ >> "$LOG" 2>&1 &

python3 /boot/home/altwatch.py "$LOG" 35,70,105,140,175,210 245 >> "$OUT" 2>&1

say ""
say "navigations=$(grep -c LoadURL "$LOG")"
say "crashes=$(grep -c 'Received signal' "$LOG")"
say "blink styles=$(grep -c computed_style "$LOG")"
say "alive=$(ps | grep -c '[c]ontent_shell')"
say "uptime at end: $(uptime)"

screenshot --silent /boot/home/newstest.png 2>/dev/null
cat "$OUT"
