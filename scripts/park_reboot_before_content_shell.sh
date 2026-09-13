#!/bin/sh
# Wait for the ozone phase to finish, then reboot before content_shell starts.
#
# The pipeline script runs ozone and content_shell back to back, so the only
# safe place to insert a reboot is the moment the phase file flips. This
# watcher polls for that flip, stops the watchdog *first* (otherwise it would
# resurrect the pipeline within its 300s interval), then the pipeline, then
# ninja, and reboots.
#
#   nohup sh park_reboot_before_content_shell.sh > /dev/null 2>&1 < /dev/null &
#
# To call it off, create the cancel file:
#   touch /boot/home/rchromium-native/park-reboot.cancel
set -u

overlay=/boot/home/rchromium-native
phase_file="$overlay/native-content-shell.phase"
status_file="$overlay/native-content-shell.status"
log="$overlay/park-reboot.log"
cancel="$overlay/park-reboot.cancel"
interval=30

note()
{
    printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$1" >> "$log"
}

# Haiku has no pgrep/pkill. Its ps prints the command first (padded, and it
# contains spaces) followed by four numeric columns, so the pid is $(NF-3).
pids_for()
{
    ps | grep "$1" | awk '{print $(NF-3)}' | grep -E '^[0-9]+$'
}

stop()
{
    label=$1
    pattern=$2
    for pid in $(pids_for "$pattern"); do
        note "stopping $label pid $pid"
        kill "$pid" 2>/dev/null
    done
}

note "armed; waiting for ozone -> content-shell"

while true; do
    if [ -f "$cancel" ]; then
        note "cancelled via $cancel"
        exit 0
    fi

    phase=$(cat "$phase_file" 2>/dev/null || echo '')
    status=$(cat "$status_file" 2>/dev/null || echo '')

    # A failure before content_shell means ozone never finished; leave the
    # machine up so the error can be read.
    case "$status" in
    'failed verify-no-qt'*|'failed gn-gen'*|'failed ozone'*)
        note "pipeline failed ($status) before content_shell; not rebooting"
        exit 1
        ;;
    esac

    if [ "$phase" = 'content-shell' ] || [ "$phase" = 'complete' ]; then
        note "ozone finished (phase=$phase); shutting the pipeline down"

        # Order matters: the watchdog restarts anything it finds stopped.
        stop watchdog '[w]atchdog_native_content_shell87'
        sleep 3
        stop pipeline '[c]ontinue_native_content_shell87'
        sleep 3
        stop ninja '[n]inja'
        sleep 5
        stop compiler '[g]++-x86'
        sleep 3

        # Rebooting this machine while an SSH session is attached reliably
        # panics it: killing the sshd team unbinds a TCP endpoint that is not
        # in the hash, and the KDL happens instead of a clean stop --
        #
        #   PANIC: bound endpoint 0x... not in hash!
        #   EndpointManager::Unbind -> ~TCPEndpoint -> tcp_uninit_protocol
        #     -> ~net_socket_private -> team_delete_team -> thread_exit
        #
        # That is an upstream TCP bug, not a shutdown bug, and it is exactly
        # the unclean stop this whole script exists to avoid. So: wait for
        # sessions to clear, then detach the shutdown from any session at all
        # and exit immediately, leaving nothing of ours attached either.
        #
        # The reboot this script performed on 2026-08-17 survived only because
        # it happened to fire with nobody connected. Do not rely on luck twice.
        waited=0
        while [ "$(ps | grep -c '[s]shd')" -gt 1 ] && [ "$waited" -lt 300 ]; do
            [ "$waited" = 0 ] && note "SSH session attached; waiting for it to drop before rebooting"
            sleep 10
            waited=$((waited + 10))
        done
        if [ "$(ps | grep -c '[s]shd')" -gt 1 ]; then
            note "SSH session still attached after ${waited}s; NOT rebooting (would panic)"
            note "build is stopped and safe; reboot by hand with no session attached"
            exit 1
        fi

        note "flushing and rebooting detached"
        sync
        nohup sh -c 'sleep 5; sync; shutdown -r -q' > /dev/null 2>&1 < /dev/null &
        exit 0
    fi

    sleep "$interval"
done
