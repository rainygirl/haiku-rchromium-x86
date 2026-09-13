/* Thread names, states and CPU time for one team.
 *
 * ps and top both truncate the thread name to something useless here
 * ("content_shell pt"), and there is no gdb for x86 on this machine, so when
 * a stalled content_shell burns 76% of both cores there is no way to say
 * which thread is doing it. get_next_thread_info() carries the full name and
 * the scheduler state, which is exactly what is needed to tell a spinning
 * thread from a waiting one. (Do not reach for `profile` for this: it
 * panicked the kernel on 2026-09-12.) */
#include <OS.h>
#include <stdio.h>
#include <stdlib.h>

static const char* state_name(thread_state s) {
    switch (s) {
        case B_THREAD_RUNNING:   return "RUNNING";
        case B_THREAD_READY:     return "READY";
        case B_THREAD_RECEIVING: return "RECEIVING";
        case B_THREAD_ASLEEP:    return "ASLEEP";
        case B_THREAD_SUSPENDED: return "SUSPENDED";
        case B_THREAD_WAITING:   return "WAITING";
        default:                 return "?";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: threadinfo <team>\n"); return 2; }
    team_id team = (team_id)atoi(argv[1]);
    int32 cookie = 0;
    thread_info info;
    int n = 0;
    printf("%-8s %-32s %-10s %10s %10s %8s\n",
           "tid", "name", "state", "user_ms", "kernel_ms", "sem");
    while (get_next_thread_info(team, &cookie, &info) == B_OK) {
        printf("%-8ld %-32s %-10s %10lld %10lld %8ld\n",
               (long)info.thread, info.name, state_name(info.state),
               (long long)(info.user_time / 1000),
               (long long)(info.kernel_time / 1000), (long)info.sem);
        n++;
    }
    if (n == 0) fprintf(stderr, "no threads for team %ld\n", (long)team);
    return 0;
}
