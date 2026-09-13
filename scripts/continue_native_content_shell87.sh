#!/bin/sh
set -u

checkout=${1:-/boot/home/rchromium-chromium87-fast}
overlay=${2:-/boot/home/rchromium-native}
chromium="$checkout/chromium"
out="$chromium/out/rchromium_native"
gn="$checkout/gn/out/gn"
phase_file="$overlay/native-content-shell.phase"
status_file="$overlay/native-content-shell.status"

PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig
export PKG_CONFIG_PATH

# g++-x86/gcc-x86 wrappers that add --param ggc-min-expand=10
# --param ggc-min-heapsize=32768 to compiles, so two large jumbo units fit
# in 2 GB at -j2. A PATH wrapper on purpose: putting the flags in the GN
# cflags would change ninja's command strings and rebuild all ~10,000
# objects. Remove this line to go back to the plain compiler.
PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH
export PATH

record_failure()
{
    code=$1
    printf 'failed %s %s\n' "$(cat "$phase_file")" "$code" > "$status_file"
    exit "$code"
}

: > "$status_file"
printf 'verify-no-qt\n' > "$phase_file"
make -C "$overlay" verify-no-qt || record_failure $?

printf 'gn-gen\n' > "$phase_file"
(cd "$chromium" && "$gn" gen "$out") || record_failure $?

# -j1, reverted from -j2 on 2026-08-18 after -j2 wedged the machine. Two large
# jumbo translation units compiled concurrently (cc_jumbo_1.o, which merges 44
# sources, and sqlite3_shim.o) exhausted RAM on this 2 GB box: the kernel looped
# on "low resource pages", both objects sat at 0 bytes for 88 minutes, and both
# compilers became unkillable by SIGTERM and SIGKILL alike. -j2 had only ever
# been measured over small media objects, never through cc or Blink jumbo TUs.
# Do not raise this again without first lowering jumbo_file_merge_limit, and note
# that -j2 also coincided with SSH unresponsiveness back on 2026-08-13.
# The -j value is not part of any compile command line, so changing it never
# invalidates .ninja_log; changing it costs only a restart.
# The second logical CPU on this machine comes up only on some boots (see the
# VAIO P notes on AP bring-up), so pin -j to what this boot actually has rather
# than to a number picked on an earlier boot. On one CPU, -j2 buys no throughput
# and doubles peak memory, which is the thing this build cannot afford.
jobs=$(sysinfo -cpu 2>/dev/null | grep -c '^CPU #')
case "$jobs" in
	1|2) ;;
	*) jobs=1 ;;
esac
#
# The wedge the comment above used to warn about -- cc1plus at 0% CPU
# surviving SIGKILL, every write to /boot blocked afterwards -- was the
# modified-page quota deadlock in the kernel's page writer, and the kernel
# running here carries the fix. That specific defect is not a reason to run
# -j1.
#
# Ordinary page exhaustion through blink core (repeated "low resource pages:
# critical", and on 2026-08-23 a failed IOBuffer::LockMemory(1890 pages)
# panic) is handled by the ggc wrappers on the PATH above, which cut peak
# compiler memory enough that the heaviest units got both quieter and faster
# (core_generated_jumbo_12: 24.29 min -> 17.94 min). That covers one
# compiler's own memory appetite. It does not cover two already-large
# compiles landing on both -j2 slots at once -- that pairing is what
# repeatedly forced content/browser's build (0061, 0062) to identify and
# exclude specific outlier files from jumbo, and content/browser's remaining
# jumbo units (60s-80s range, several taking 60-130 minutes) kept driving
# free memory to single digits of MB, several times a day, even after those
# fixes landed.
#
# On 2026-09-01 this culminated in a hard halt with no panic trace: disk
# writes silently stopped landing at some point, the machine kept computing
# in memory for roughly two more hours, then halted outright. syslog was
# empty across the whole gap; the next boot logged bfs index corruption
# ("Could not find value in index"), consistent with an unclean stop rather
# than an orderly panic. Cause not conclusively identified (aging hardware
# and sustained memory pressure both plausible, no SMART tooling available
# on this system to check the disk directly) but 1.5-2 hours of already-
# completed compiles (browser_jumbo_78 through 82) were lost because their
# writes never reached disk before the halt.
#
# -j1 guarantees no two compiles ever run concurrently, which removes the
# mechanism behind every incident above at the cost of half the throughput.
# Given a wedge or halt costs a reboot, manual recovery, and whatever writes
# had not reached disk, that trade is worth it here. RCHROMIUM_JOBS=2 can
# still force -j2 back on if a future fix (further jumbo exclusions, more
# RAM, a swap file) removes the underlying pressure.
jobs=1
case "${RCHROMIUM_JOBS:-}" in
	1|2) jobs=$RCHROMIUM_JOBS ;;
esac
echo "using -j$jobs ($(sysinfo -cpu 2>/dev/null | grep -c '^CPU #') logical cpu(s) detected)"

printf 'ozone\n' > "$phase_file"
ninja -C "$out" -j$jobs -k 20 'haiku_port/ozone:ozone' || record_failure $?

printf 'content-shell\n' > "$phase_file"
ninja -C "$out" -j$jobs -k 20 content_shell || record_failure $?

printf 'complete\n' > "$phase_file"
printf 'complete 0\n' > "$status_file"
