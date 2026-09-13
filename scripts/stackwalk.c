/* Print the call stack of live threads in another team.
 *
 * The one thing this port has lacked is a way to see *where* a thread is
 * when it misbehaves: there is no gdb for x86 Haiku on this machine and the
 * kernel profiler panics on content_shell. This uses the user-space debugger
 * API instead -- attach, stop one thread, read its instruction pointer, walk
 * the frame-pointer chain (the build is -fno-omit-frame-pointer), resume it.
 * Addresses are printed as " [0x...]" so scripts/resolve_haiku_stack.py can
 * symbolize them against `listimage <team>` taken from the same run.
 *
 * usage: stackwalk <team> <thread> [<thread>...]
 */
#include <OS.h>
#include <debugger.h>
#include <debug_support.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void continue_thread(port_id nub, thread_id t) {
    debug_nub_continue_thread c;
    memset(&c, 0, sizeof(c));
    c.thread = t;
    c.handle_event = B_THREAD_DEBUG_HANDLE_EVENT;
    c.single_step = false;
    write_port(nub, B_DEBUG_MESSAGE_CONTINUE_THREAD, &c, sizeof(c));
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: stackwalk <team> <thread>...\n"); return 2; }
    team_id team = atoi(argv[1]);

    port_id port = create_port(16, "stackwalk debugger port");
    port_id nub = install_team_debugger(team, port);
    if (nub < 0) { fprintf(stderr, "install_team_debugger: %s\n", strerror(nub)); return 1; }
    debug_context ctx;
    if (init_debug_context(&ctx, team, nub) != B_OK) { fprintf(stderr, "init_debug_context failed\n"); return 1; }

    for (int a = 2; a < argc; ++a) {
        thread_id tid = atoi(argv[a]);
        thread_info ti;
        get_thread_info(tid, &ti);
        printf("=== thread %ld \"%s\"\n", (long)tid, ti.name);
        if (debug_thread(tid) != B_OK) { printf("  debug_thread failed\n"); continue; }

        /* Wait until *this* thread reports in; resume anything else that
         * stops in the meantime so the team is not left half-frozen. */
        int got = 0;
        for (int tries = 0; tries < 50 && !got; ++tries) {
            int32 code;
            debug_debugger_message_data msg;
            ssize_t n = read_port_etc(port, &code, &msg, sizeof(msg), B_RELATIVE_TIMEOUT, 200000);
            if (n < 0) continue;
            if (msg.origin.thread == tid) { got = 1; break; }
            if (msg.origin.thread > 0 && msg.origin.nub_port >= 0)
                continue_thread(nub, msg.origin.thread);
        }
        if (!got) { printf("  thread did not stop\n"); continue; }

        void* ip = NULL; void* frame = NULL;
        if (debug_get_instruction_pointer(&ctx, tid, &ip, &frame) == B_OK)
            printf(" [0x%08lx]\n", (unsigned long)ip);
        void* prev = NULL;
        for (int i = 0; i < 48 && frame != NULL && frame != prev; ++i) {
            debug_stack_frame_info fi;
            if (debug_get_stack_frame(&ctx, frame, &fi) != B_OK) break;
            if (fi.return_address == NULL) break;
            printf(" [0x%08lx]\n", (unsigned long)fi.return_address);
            prev = frame;
            frame = fi.parent_frame;
            if (frame != NULL && frame <= prev) break;   /* chain must go up */
        }
        continue_thread(nub, tid);
    }
    destroy_debug_context(&ctx);
    remove_team_debugger(team);
    return 0;
}
