#!/bin/sh
# Validate the RandBytes buffering fix: launch reliability, then navigation.
#
# Before the fix the same binary rendered on 3/6, 3/6, 1/6, 0/6 and 0/4
# launches depending on the day's luck; a failed launch had exactly one
# PresentCanvas and zero computed_style messages. Success here means every
# launch renders, and alternating the two news sites keeps rendering.
OUT=/boot/home/validate.txt
: > "$OUT"
say() { printf '%s\n' "$*" >> "$OUT"; }
kill_shell() { t=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}'); [ -n "$t" ] && kill "$t"; sleep 3; }

sh /boot/home/install_to_desktop.sh > /dev/null 2>&1
say "uptime: $(uptime)"
say "=== launch reliability (news.naver.com, 28 s each)"
ok=0
for n in 1 2 3 4 5 6; do
    kill_shell
    L=/boot/home/val$n.log; : > "$L"
    ( "/boot/home/Desktop/R Chromium" https://news.naver.com/ >> "$L" 2>&1 ) &
    sleep 28
    st=$(grep -c computed_style "$L"); pa=$(grep -c PresentCanvas "$L")
    sig=$(grep -m1 -o "Received signal [0-9]* [A-Z_]*" "$L")
    [ "$st" -gt 0 ] && ok=$((ok + 1))
    say "  run$n styles=$st paints=$pa ${sig:-no-crash}"
done
say "  rendered $ok / 6"

say "=== alternating navigation via Shell::LoadURL (address-bar path)"
kill_shell
L=/boot/home/valalt.log; : > "$L"
NAV="30:https://news.google.co.kr/,65:https://news.naver.com/,100:https://news.google.co.kr/,135:https://news.naver.com/"
( RCH_NAV_AFTER="$NAV" "/boot/home/Desktop/R Chromium" https://news.naver.com/ >> "$L" 2>&1 ) &
python3 /boot/home/altwatch.py "$L" 30,65,100,135 170 >> "$OUT" 2>&1
say "  navigations=$(grep -c LoadURL "$L") crashes=$(grep -c 'Received signal' "$L") styles=$(grep -c computed_style "$L") alive=$(ps | grep -c '[c]ontent_shell')"
screenshot --silent /boot/home/validate.png 2>/dev/null
say "=== thread names now (SetName fix):"
t=$(ps | grep "[c]ontent_shell" | head -1 | awk '{print $(NF-3)}')
[ -n "$t" ] && /boot/home/threadinfo "$t" | awk 'NR>1 {print $2}' | sort | uniq -c | sort -rn | head -12 >> "$OUT"
say "VALIDATE-DONE"
