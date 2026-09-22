/* Does Haiku honour pthread_attr_setstacksize?
 *
 * Patch 0090 returns 4 MB from base::GetDefaultThreadStackSize() because
 * Haiku's default for a non-main thread is USER_STACK_SIZE (256 kB) and V8
 * assumes something like glibc's 8 MB. base::CreateThread passes that to
 * pthread_attr_setstacksize. Whether the kernel then gives the thread 4 MB is
 * a separate question, and the x.com crash -- a SIGSEGV at address 0 on
 * Chrome_InProcRendererThread, under nineteen stacked V8 interpreter frames --
 * looks exactly like the one that patch was written for.
 *
 * Two answers, because they can disagree: what the attribute reads back as,
 * and how far a recursion actually gets before the thread dies.
 *
 * Build: gcc-x86 -O0 -o stacksize stacksize.c  (32-bit, like content_shell)
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile char* g_lowest;
static char* g_base;

static void burn(int depth) {
    char frame[1024];
    memset(frame, depth & 0xff, sizeof frame);
    g_lowest = frame;
    if (depth < 100000) {
        burn(depth + 1);
    }
}

static void* child(void* arg) {
    char here;
    size_t want = (size_t)(uintptr_t)arg;
    pthread_attr_t got;
    size_t got_size = 0;

    g_base = &here;
    if (pthread_getattr_np(pthread_self(), &got) == 0) {
        pthread_attr_getstacksize(&got, &got_size);
        printf("  requested %zu, attribute reads back %zu\n", want, got_size);
    } else {
        printf("  requested %zu, pthread_getattr_np unavailable\n", want);
    }
    fflush(stdout);
    return NULL;
}

static void* child_burn(void* arg) {
    char here;
    (void)arg;
    g_base = &here;
    g_lowest = &here;
    burn(0);
    return NULL;
}

static void try_size(size_t want) {
    pthread_t t;
    pthread_attr_t a;
    printf("stack request %zu bytes (%zu kB)\n", want, want / 1024);
    pthread_attr_init(&a);
    if (want) {
        int rc = pthread_attr_setstacksize(&a, want);
        if (rc != 0) {
            printf("  pthread_attr_setstacksize -> %d (%s)\n", rc, strerror(rc));
        }
    }
    if (pthread_create(&t, &a, child, (void*)(uintptr_t)want) != 0) {
        printf("  pthread_create FAILED\n");
        return;
    }
    pthread_join(t, NULL);
    pthread_attr_destroy(&a);
}

int main(void) {
    size_t sizes[] = {0, 256u * 1024, 1024u * 1024, 4u * 1024 * 1024,
                      8u * 1024 * 1024};
    size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
        try_size(sizes[i]);
    }

    /* Now the one that matters: ask for 4 MB and see how deep a 1 kB-frame
     * recursion gets before the thread is killed. 4 MB should be about 4000
     * frames; 256 kB is about 250. */
    printf("\nrecursion depth on a 4 MB request:\n");
    fflush(stdout);
    {
        pthread_t t;
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 4u * 1024 * 1024);
        if (pthread_create(&t, &a, child_burn, NULL) == 0) {
            pthread_join(t, NULL);
            printf("  survived the whole recursion (100000 frames)\n");
        }
    }
    return 0;
}
