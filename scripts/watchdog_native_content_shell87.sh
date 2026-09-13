#!/bin/sh
# Keeps the durable R Chromium pipeline alive across the days the
# content_shell phase needs, without an SSH session being attached.
#
# It restarts the pipeline only when the previous run made progress. A run
# that stops at the same edge it stopped at last time means a source error
# that needs a human, so the watchdog parks instead of looping forever.
set -u

overlay=/boot/home/rchromium-native
checkout=/boot/home/rchromium-chromium87-fast
pipeline="$overlay/scripts/continue_native_content_shell87.sh"
out="$checkout/chromium/out/rchromium_native"

log_file="$overlay/native-content-shell.log"
phase_file="$overlay/native-content-shell.phase"
status_file="$overlay/native-content-shell.status"

watch_log="$overlay/watchdog.log"
state_file="$overlay/watchdog.state"
interval=300
max_restarts=200

# No .ninja_log growth for this long, with no build tool running, is a stall.
# Generous on purpose: the longest single unit measured is 24.29 minutes
# (core_generated_jumbo_12), and a final link may take longer still -- but a
# link keeps a toolchain process alive, which is what the second test checks.
stall_minutes=30

note()
{
    printf "%s %s\n" "$(date "+%Y-%m-%d %H:%M:%S")" "$1" >> "$watch_log"
}

# Highest edge number reached in the current log, e.g. "[283/14732]" -> 283.
current_edge()
{
    grep -E "^\[[0-9]+/[0-9]+\]" "$log_file" 2>/dev/null \
        | tail -1 | sed -e "s/^\[//" -e "s|/.*||"
}

# Haikus ps truncates the command column near 63 characters, and it also
# lists the shell running THIS script -- whose command line contains every
# pattern searched for below. Excluding the interpreter lines is what keeps a
# search for "ninja" from always finding itself.
# One ps per pass, held in a variable, instead of a pipeline per question.
#
# This is not tidiness. Every $( ) and every stage of a pipeline is a forked
# subshell, and on 2026-08-26 the shell those forks run in was crashing: 54
# reports on the Desktop, all of them /boot/system/bin/sh, all at the same
# instruction in libhistory's destructor, reached through command_substitute ->
# exit. They arrived in bursts at 07:06, 07:12 and 07:17 -- exactly 300 seconds
# apart, which is this loop's interval -- and the watchdog was dead afterwards.
# The supervisor was being killed by the same fault it exists to survive.
#
# Cutting the forks per pass from twenty-five or so to about six does not fix
# that bash bug (see AGENTS.md for how far that was traced), but it takes most
# of the exposure away.
snapshot=""
ninja_seen_at=
quiet_passes=0

take_snapshot()
{
    snapshot=$(ps | grep -v "bin/bash -c" | grep -v "[s]cripts/watchdo")
}

# Answer from the snapshot with the shell's own pattern matching -- no fork.
in_snapshot()
{
    case "$snapshot" in
        *"$1"*) return 0 ;;
        *)      return 1 ;;
    esac
}

running()
{
    take_snapshot
    in_snapshot "$1"
}

# These read whatever snapshot is current; the loop refreshes it once per pass.
ninja_running()   { in_snapshot "ninja"; }
pipeline_running(){ in_snapshot "scripts/contin"; }
tools_running()   { in_snapshot "i586-pc-haiku"; }

pid_of()
{
    ps | grep -v "bin/bash -c" | grep -v "[s]cripts/watchdo" \
        | grep "$1" | head -1 | awk "{print \$(NF-3)}"
}

# Minutes since .ninja_log last grew. That file is the only authority on
# progress: the text log is truncated on every restart, and a tail of it looks
# identical whether the build is working or wedged.
idle_minutes()
{
    m=$(stat -c %Y "$out/.ninja_log" 2>/dev/null || echo 0)
    [ "$m" = 0 ] && { echo 0; return; }
    echo $(( ( $(date +%s) - m ) / 60 ))
}

# Edges recorded since the build began. Unlike the [n/m] counter in the text
# log this only ever grows: ninja renumbers from 1 on every invocation and the
# text log is truncated on every restart, so the two are not comparable across
# runs. This is the same file the stall test trusts, for the same reason.
log_entries()
{
    wc -l < "$out/.ninja_log" 2>/dev/null || echo 0
}


# ninja appends to .ninja_deps continuously and rewrites it on load. Killing
# ninja mid-write truncates it, and every object whose deps record was lost is
# recompiled even though the object is still on disk -- on 2026-08-24 that cost
# roughly 14 hours of repeated work in blink core. Copy it before any kill so
# the damage is at worst recoverable, and always say what the sizes were.
stop_ninja()
{
    pid=$(pid_of "[n]inja")
    [ -n "$pid" ] || return 0

    before=$(stat -c %s "$out/.ninja_deps" 2>/dev/null || echo 0)
    cp "$out/.ninja_deps" "$out/.ninja_deps.pre-kill" 2>/dev/null
    note "stopping ninja $pid (.ninja_deps $before bytes, copied to .ninja_deps.pre-kill)"

    # SIGINT first: ninja handles it and finishes writing its logs. Only
    # escalate when it does not, and give each step long enough to matter.
    for sig in INT TERM KILL; do
        kill -$sig "$pid" 2>/dev/null
        n=0
        while [ $n -lt 20 ]; do
            # Refresh: in_snapshot() reads whatever take_snapshot() last saw,
            # so a wait loop has to re-take it or it spins on a stale answer.
            take_snapshot
            ninja_running || break
            sleep 3
            n=$((n + 1))
        done
        take_snapshot
        ninja_running || { note "ninja stopped by SIG$sig"; break; }
        note "ninja survived SIG$sig; escalating"
    done

    after=$(stat -c %s "$out/.ninja_deps" 2>/dev/null || echo 0)
    note "after stop: .ninja_deps $after bytes (was $before)"
}

restarts=0
last_restart_edge=-1
last_restart_entries=-1
last_restart_phase=""

# Refuse to be the second watchdog.
#
# Two supervisors decide independently when to restart the pipeline, so the
# moment the build stops they race and start two of them -- two ninjas in one
# output directory, the one thing this project forbids.
#
# The first version of this guard parsed a pid out of a ps column with
# $(NF-3). That broke on 2026-08-25: Haiku's ps truncates the command near 63
# characters, "/boot/system/bin/sh scripts/watchdog_native_content_shell87.sh"
# is 62, and the truncation changed the field count, so the guard read the
# wrong number, decided it was somebody else, and refused to let any watchdog
# start at all -- worse than the problem it was added for.
#
# So no column parsing. A pid file says who claims the role, kill -0 says
# whether that process still exists, and a match against ps says it is still
# this script rather than a reused pid.
#
# That ps match has to respect the same truncation. On 2026-09-08 this guard
# read "watchdog_native", which never appears in ps output at all -- the
# command column stops at ".../scripts/watchdo", one character short of the
# "g". So the guard concluded no watchdog was running, every launch was let
# through, and three supervisors ended up polling the same tree at once.
# Nothing detected it either: `ps | grep watchdog` finds nothing for the same
# reason, which is what made three consecutive launches each look like a
# failure to start. Match on "scripts/watchdo" -- inside the cut -- and check
# liveness through the pid file, never through a grep for the full name.
#
# The ps side of the check cannot come from take_snapshot(): that helper
# deliberately filters watchdog lines out, so asking it whether a watchdog is
# running always answers no. Ask ps directly. This runs once, at startup, not
# once per pass, so it costs nothing against the fork-crash problem described
# above.
pid_file="$overlay/watchdog.pid"
if [ -f "$pid_file" ]; then
    other=$(cat "$pid_file" 2>/dev/null)
    others=$(ps | grep "[s]cripts/watchdo" | wc -l)
    if [ -n "$other" ] && [ "$other" != "$$" ] && kill -0 "$other" 2>/dev/null \
        && [ "$others" -gt 1 ]; then
        note "another watchdog is already running (pid $other); exiting"
        exit 0
    fi
fi
printf '%s\n' "$$" > "$pid_file"

note "watchdog started (interval ${interval}s, max ${max_restarts} restarts, stall ${stall_minutes}min)"

while true; do
    # Force dirty pages out on every pass. On 2026-09-01 this machine lost two
    # separate full days of build output: after a hard halt in the morning and
    # a link-stage panic that evening, every file under out/rchromium_native
    # came back dated 09:03, and files scp'd in at 18:00 were simply absent.
    # checkfs -c found the volume structurally clean (0 blocks unallocated,
    # 0 already set, 0 freeable across 396,600 nodes), so this is not disk or
    # bfs corruption: the kernel's page writer stops making progress under
    # sustained build I/O and dirty pages accumulate in RAM until something
    # crashes, at which point hours of completed compiles evaporate at once.
    #
    # Backgrounded on purpose. If the page writer is already wedged this sync
    # will block indefinitely, and the watchdog must stay responsive enough to
    # notice a stall and restart ninja; a foreground sync would take the
    # supervisor down with it. Losing at most one interval (5 min) of output
    # instead of a whole day is worth one cheap call per pass.
    sync &

    take_snapshot
    phase=$(cat "$phase_file" 2>/dev/null)
    status=$(cat "$status_file" 2>/dev/null)

    case "$status" in
    "complete 0")
        note "build complete; watchdog exiting"
        printf "complete\n" > "$state_file"
        exit 0
        ;;
    esac

    if ninja_running || pipeline_running; then
        # Alive is not the same as working. On 2026-08-24 at 23:06 ninja stopped
        # collecting finished compiles: the compilers exited normally and their
        # objects landed on disk, but .ninja_log was never appended to again and
        # no new work was started. strace showed ninja issuing no syscalls at
        # all. Nothing reached the syslog, memory was fine, /boot was writable.
        # The old test here was ninja_running alone, so the watchdog slept
        # through it and would have slept through it indefinitely.
        #
        # Two conditions, and ninja_running is not optional. gn gen runs with
        # the pipeline up but no ninja, no toolchain process, and a .ninja_log
        # left stale by the previous run -- which satisfies every other test
        # here. An earlier version of this omitted it and fell through to the
        # restart below, launching a second pipeline on top of the one still
        # running gn gen. Two ninjas in one output directory is the single
        # thing this project forbids outright.
        idle=$(idle_minutes)

        # The idle figure is the age of .ninja_log, which says nothing about how
        # long *this* ninja has been up. After a reboot that file is hours old
        # before the build has run for a second, so on 2026-08-28 a ninja that
        # had been alive for two minutes -- still loading a 16 MB .ninja_deps,
        # no compiler spawned yet -- was reported as "no progress in 315 min"
        # and killed, twice, on the way to a restart loop.
        #
        # So the test only means something once the current run has had time to
        # produce an edge of its own. Give it the same window the stall test
        # uses, measured from when this pass first saw ninja.
        if ninja_running; then
            [ -n "$ninja_seen_at" ] || ninja_seen_at=$(date +%s)
            ninja_up=$(( ( $(date +%s) - ninja_seen_at ) / 60 ))
        else
            ninja_seen_at=
            ninja_up=0
        fi

        if ninja_running && [ "$idle" -ge "$stall_minutes" ] \
            && [ "$ninja_up" -ge "$stall_minutes" ] && ! tools_running; then
            note "STALL: ninja alive ${ninja_up} min, .ninja_log has not grown in ${idle} min and no toolchain process is running"
            stop_ninja
        fi

        # Whatever was decided above, never fall through while something is
        # still up. Killing ninja lets the pipeline script return and exit on
        # its own; the next pass finds the machine idle and restarts cleanly,
        # with the no-progress guard applied as usual.
        sleep "$interval"
        continue
    fi

    # Nothing is running. Decide whether restarting can help.
    edge=$(current_edge)
    [ -n "$edge" ] || edge=0

    if [ "$restarts" -ge "$max_restarts" ]; then
        note "restart budget exhausted at edge $edge; parking"
        printf "parked restart-budget edge=%s phase=%s\n" "$edge" "$phase" \
            > "$state_file"
        exit 1
    fi

    # Only the ninja phases report edges, and each restart truncates the log,
    # so a stall in verify-no-qt or gn-gen shows edge 0 and would look like
    # regression from the previous phases edge count. Reaching a different
    # phase than the last restart is progress in its own right.
    # Measured on .ninja_log, not on the [n/m] counter.
    #
    # On 2026-08-26 that distinction cost three and a half hours. The stall
    # detector had just done its job perfectly at 04:12 -- caught a wedged
    # ninja, backed up the deps log, escalated INT/TERM/KILL, lost nothing --
    # and then this test threw the recovery away at 04:19: the restarted run
    # had reached edge 197 while the run before it died at 244, so a fresh
    # start looked like regression. It always will, because ninja renumbers
    # from 1 each time. Entries only ever accumulate.
    entries=$(log_entries)

    # An empty phase is not a phase. The pipeline writes that file as its first
    # act, so a watchdog pass that lands in the gap between launch and that
    # write reads "" -- and "" equals the "" remembered from the same gap last
    # time, which satisfied the test below and parked a perfectly healthy tree
    # on 2026-08-26 (recorded as `phase=` in watchdog.state). Absence of
    # information is not evidence of no progress; wait for the next pass.
    if [ -z "$phase" ]; then
        note "phase not written yet; deferring the progress test"
        sleep "$interval"
        continue
    fi

    # Parking needs more than one quiet pass. A restart does not begin
    # compiling for several minutes: ninja reads a .ninja_deps that is now over
    # 20 MB, then gn's stamp checks run, and only then does a compiler appear --
    # measured repeatedly at four to five minutes on this machine, against a
    # 300-second poll interval. Testing after a single pass therefore asks
    # whether the build finished work it has not been given time to start, and
    # it parked a healthy tree twice on 2026-08-29 alone (04:35 and 14:47),
    # costing about three hours each time because parking waits for a human.
    #
    # Two consecutive passes with no new edge is still only ten minutes, which
    # is well short of the stall threshold and cannot mask a real failure: a
    # restart that genuinely cannot make progress fails the same test again.
    if [ "$entries" -le "$last_restart_entries" ] \
        && [ "$phase" = "$last_restart_phase" ]; then
        quiet_passes=$((quiet_passes + 1))
        if [ "$quiet_passes" -lt 2 ]; then
            note "no new edges yet after restart (pass $quiet_passes); giving it another interval before parking"
            sleep "$interval"
            continue
        fi
        note "no progress past $entries recorded edges in phase $phase since last restart; parking for a human"
        printf "parked no-progress entries=%s phase=%s status=%s\n" \
            "$entries" "$phase" "$status" > "$state_file"
        exit 1
    fi

    stamp=$(date "+%Y%m%d-%H%M%S")
    if [ -f "$log_file" ]; then
        cp "$log_file" "$log_file.$stamp"
    fi

    # Last line of defence, immediately before the launch: re-check that the
    # machine really is idle. Everything above ran minutes ago and the cost of
    # being wrong here is a corrupted build tree, not a wasted cycle.
    if ninja_running || pipeline_running; then
        note "refusing to start: something is already running (ninja/pipeline)"
        sleep "$interval"
        continue
    fi

    restarts=$((restarts + 1))
    quiet_passes=0
    last_restart_entries=$entries
    last_restart_edge=$edge
    last_restart_phase=$phase
    note "restart #$restarts from edge $edge (phase=$phase status=$status)"
    printf "running restarts=%s edge=%s\n" "$restarts" "$edge" > "$state_file"

    # RCHROMIUM_LINK_LOWMEM is not optional here. The g++-x86 wrapper on the
    # PATH reads it to decide whether to strip -Wl,--gc-sections and -Wl,-O2
    # from the content_shell link, and only from that link. Without it ld
    # builds its whole-program reachability graph before writing anything and
    # is killed by the kernel partway through the ~190 MB output -- measured on
    # 2026-09-03, dead at 141,614,983 bytes with "ld terminated with signal 21
    # [Kill Thread]". A watchdog that restarts the pipeline without this
    # restarts it into that failure every time.
    # RCHROMIUM_KEEP_GC belongs with it. Dropping --gc-sections makes the
    # content_shell output 172,967,599 bytes and ld was killed writing that out
    # every time it was tried; keeping the flag brings the intermediate down to
    # 141,639,559 and is the only configuration that has ever produced a
    # finished binary on this machine (2026-09-10, twice). A watchdog that
    # restarts the pipeline without it restarts it into the failing shape.
    RCHROMIUM_LINK_LOWMEM=1 RCHROMIUM_KEEP_GC=1 \
        nohup sh "$pipeline" "$checkout" "$overlay" > "$log_file" 2>&1 < /dev/null &

    # Settle longer than a gn-gen takes; the measured run was 152s, so the old
    # 60s could reach the checks above while gn was still working.
    sleep 300
done
