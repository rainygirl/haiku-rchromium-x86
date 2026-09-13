#!/bin/sh
# One-command health report for the content_shell build.
#
# Read this top to bottom: liveness, then supervision, then pressure. Anything
# below the first two sections can look perfectly healthy while the build is
# dead, which is how a 33-minute stall went unreported on 2026-08-24.
out=/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native
overlay=/boot/home/rchromium-native
log=$overlay/native-content-shell.log

# ps truncates commands near 63 chars, and lists the shell running this script
# -- whose command line contains every pattern searched for. Filter both.
procs() { ps | grep -v "bin/bash -c" | grep -v "[m]emcheck" | grep -c "$1"; }

echo "== $(date "+%H:%M:%S")  up$(uptime | sed "s/.*up//;s/,.*//")"
echo "   $(tail -1 $log)"

# .ninja_log is the only authority on progress: the text log above is truncated
# on every watchdog restart, and its last line reads the same whether the build
# is working or wedged.
now=$(date +%s)
mtime=$(stat -c %Y $out/.ninja_log 2>/dev/null || echo 0)
idle=$(( (now - mtime) / 60 ))
ninja=$(procs "[n]inja")
tools=$(procs "[i]586-pc-haiku")
entries=$(wc -l < $out/.ninja_log)

state="ok"
if [ "$ninja" -eq 0 ]; then
	state="NO NINJA -- build is not running"
elif [ "$idle" -ge 30 ] && [ "$tools" -eq 0 ]; then
	state="STALLED -- ninja alive, no toolchain process, no progress ${idle}min"
elif [ "$idle" -ge 30 ]; then
	state="suspicious -- no .ninja_log progress ${idle}min (tools still up)"
fi
echo "   progress:  $entries edges, last $idle min ago  [$state]"

# Never act on this machine without knowing what already supervises it: the
# watchdog restarts the pipeline by itself, so a manual relaunch races it.
wd=$(ps | grep -v "bin/bash -c" | grep -c "[w]atchdog_native")
echo "   watchdog:  $(if [ "$wd" -gt 0 ]; then echo "running"; else echo "NOT RUNNING"; fi) -- $(cat $overlay/watchdog.state 2>/dev/null || echo "no state")"
echo "   pipeline:  $(procs "[s]cripts/contin") script, $ninja ninja, $tools toolchain"

echo "   ggc wrapper active: $(procs "[g]gc-min") of $tools cc1plus"
vmstat 2>/dev/null | awk "/free memory|free swap/ {printf \"   %-12s %6.0f MB\n\", \$2, \$NF/1048576}"
echo "   low-resource messages: $(grep -c "low resource" /var/log/syslog)"
echo "   recent unit times:"
tail -4 $out/.ninja_log | awk -F"\t" "{printf \"     %6.2f min  %s\n\", (\$2-\$1)/60000, \$4}"
