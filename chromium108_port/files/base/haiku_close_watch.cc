// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Where standard input goes.
//
// Shared memory regions in this port keep arriving holding fd 0:
// PlatformSharedMemoryRegion::Create() calls mkstemp(), which honestly
// returns the lowest free descriptor, and it returns 0. A probe at image
// load says fd 0, 1 and 2 are all open when the binary starts, so stdin is
// being given away somewhere in between -- and then closed twice, which is
// what kills the browser on x.com.
//
// -Wl,--wrap sends every close() and dup2() in this binary here first.
// Neither of them turned out to be the one that took fd 0, so this also
// watches for the moment fd 0 stops being open and reports the call it
// happened next to, which brackets the event between two calls.
//
// Diagnostic. It comes out once the owner is known.

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>
#include <netdb.h>
#include <sys/socket.h>

#include <OS.h>
#include <image.h>

namespace {

addr_t ImageBase() {
  static addr_t text_base = 0;
  if (text_base == 0) {
    image_info info;
    int32 cookie = 0;
    while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
      if (info.type == B_APP_IMAGE) {
        text_base = (addr_t)info.text;
        break;
      }
    }
  }
  return text_base;
}

bigtime_t g_t0 = system_time();

// Seconds since the image loaded, so these lines can be lined up against
// Chromium's own timestamped logging.
double Now() {
  return (double)(system_time() - g_t0) / 1000000.0;
}

bool g_stdin_seen_open = true;

// fcntl(0) answering "open" does not mean fd 0 is still standard input: a
// socket on fd 0 answers the same way. Remember what fd 0 was at image load
// and say so when it becomes something else.
dev_t g_stdin_dev = -1;
ino_t g_stdin_ino = -1;
bool g_stdin_identity_known = false;
bool g_stdin_replaced_reported = false;

void RememberStdin() {
  struct stat st;
  if (fstat(0, &st) == 0) {
    g_stdin_dev = st.st_dev;
    g_stdin_ino = st.st_ino;
    g_stdin_identity_known = true;
    fprintf(stderr, "[RCH %+.3f] fd 0 at load: dev=%d ino=%lld mode=0%o\n",
            Now(), (int)st.st_dev, (long long)st.st_ino,
            (unsigned)st.st_mode);
  }
}

// Nothing in this binary closes fd 0 -- not close(), not dup2(), not
// fclose(), not freopen(), all of which are wrapped -- and yet it is gone
// before the first socket() call. A poll is the only way left to say when.
int32 StdinWatchdog(void*) {
  while (true) {
    struct stat st;
    bool gone = fstat(0, &st) != 0;
    bool changed = !gone && g_stdin_identity_known &&
                   (st.st_dev != g_stdin_dev || st.st_ino != g_stdin_ino);
    if (gone || changed) {
      fprintf(stderr, "[RCH %+.3f] WATCHDOG: fd 0 became %s\n", Now(),
              gone ? "closed" : "something else");
      return 0;
    }
    snooze(1000);
  }
  return 0;
}

void NoteStdinIfItWasReplaced(const char* what, const void* ra) {
  if (!g_stdin_identity_known || g_stdin_replaced_reported)
    return;
  struct stat st;
  if (fstat(0, &st) != 0)
    return;
  if (st.st_dev == g_stdin_dev && st.st_ino == g_stdin_ino)
    return;
  g_stdin_replaced_reported = true;
  fprintf(stderr,
          "[RCH %+.3f] fd 0 is no longer what it was at load; noticed at %s "
          "from 0x%lx\n",
          Now(), what, (unsigned long)((addr_t)ra - ImageBase()));
}

// Called from the wrappers below, after the real call. fprintf rather than
// LOG: this has to work before logging is initialised.
void NoteStdinIfItWentAway(const char* what, const void* ra) {
  if (!g_stdin_seen_open)
    return;
  if (fcntl(0, F_GETFD) != -1)
    return;
  g_stdin_seen_open = false;
  fprintf(stderr, "[RCH %+.3f] fd 0 stopped being open around %s from 0x%lx\n", Now(), what,
          (unsigned long)((addr_t)ra - ImageBase()));
}

}  // namespace

namespace {
// Runs at image load, before anything has had a chance to move fd 0.
struct RememberStdinAtLoad {
  RememberStdinAtLoad() {
    RememberStdin();
    thread_id t = spawn_thread(StdinWatchdog, "rch stdin watchdog",
                               B_NORMAL_PRIORITY, nullptr);
    if (t >= 0)
      resume_thread(t);
  }
};
RememberStdinAtLoad g_remember_stdin_at_load;
}  // namespace

extern "C" {

// Called from net/socket/socket_posix.cc, which needs the same
// image-relative addresses and has no other reason to know about images.
void RchNoteFd(const char* what, int fd, const void* ra) {
  // What this descriptor actually is right now. If socket() hands back 0
  // while fd 0 is still the character device it was at load, then the
  // descriptor was handed out while it was in use, and nothing in this
  // browser closed anything -- the bug is below us.
  struct stat st;
  const bool have = fstat(fd, &st) == 0;
  const bool same_as_load = have && g_stdin_identity_known &&
                            st.st_dev == g_stdin_dev &&
                            st.st_ino == g_stdin_ino;
  fprintf(stderr,
          "[RCH %+.3f] %s fd=%d from 0x%lx | now: dev=%d ino=%lld mode=0%o%s\n",
          Now(), what, fd, (unsigned long)((addr_t)ra - ImageBase()),
          have ? (int)st.st_dev : -1, have ? (long long)st.st_ino : -1LL,
          have ? (unsigned)st.st_mode : 0u,
          same_as_load ? "  <-- STILL THE ONE FROM LOAD" : "");
}

int __real_close(int fd);
int __real_dup2(int oldfd, int newfd);
int __real_fclose(FILE* f);
int __real_socket(int domain, int type, int protocol);
int __real_closedir(DIR* dir);
int __real_dup(int fd);
int __real_getaddrinfo(const char* node, const char* service,
                       const struct addrinfo* hints, struct addrinfo** res);
int __real_open(const char* path, int flags, ...);
FILE* __real_freopen(const char* path, const char* mode, FILE* f);

int __wrap_close(int fd) {
  const void* ra = __builtin_return_address(0);
  // For the first two seconds, every close, whatever the descriptor. fd 0
  // disappears in a 20 ms window before the first socket() and none of the
  // wrapped entry points did it, so the question is no longer "which call"
  // but "what was running".
  if (Now() < 2.0) {
    fprintf(stderr, "[RCH %+.3f] close(%d) from 0x%lx\n", Now(), fd,
            (unsigned long)((addr_t)ra - ImageBase()));
  } else if (fd >= 0 && fd <= 2) {
    fprintf(stderr, "[RCH %+.3f] close(%d) from 0x%lx\n", Now(), fd,
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  int ret = __real_close(fd);
  NoteStdinIfItWentAway("close()", ra);
  NoteStdinIfItWasReplaced("close()", ra);
  return ret;
}

int __wrap_dup2(int oldfd, int newfd) {
  const void* ra = __builtin_return_address(0);
  if (newfd >= 0 && newfd <= 2) {
    fprintf(stderr, "[RCH %+.3f] dup2(%d -> %d) from 0x%lx\n", Now(), oldfd, newfd,
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  int ret = __real_dup2(oldfd, newfd);
  NoteStdinIfItWentAway("dup2()", ra);
  return ret;
}

// fclose() closes the descriptor inside libroot, where --wrap=close cannot
// see it. fclose(stdin) is the classic way standard input disappears.
int __wrap_fclose(FILE* f) {
  const void* ra = __builtin_return_address(0);
  int fd = f != nullptr ? fileno(f) : -1;
  if (fd >= 0 && fd <= 2) {
    fprintf(stderr, "[RCH %+.3f] fclose(fd=%d) from 0x%lx\n", Now(), fd,
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  int ret = __real_fclose(f);
  NoteStdinIfItWentAway("fclose()", ra);
  NoteStdinIfItWasReplaced("fclose()", ra);
  return ret;
}

FILE* __wrap_freopen(const char* path, const char* mode, FILE* f) {
  const void* ra = __builtin_return_address(0);
  int fd = f != nullptr ? fileno(f) : -1;
  if (fd >= 0 && fd <= 2) {
    fprintf(stderr, "[RCH %+.3f] freopen(fd=%d, %s) from 0x%lx\n", Now(), fd,
            path != nullptr ? path : "(null)",
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  FILE* ret = __real_freopen(path, mode, f);
  NoteStdinIfItWasReplaced("freopen()", ra);
  return ret;
}

// closedir() closes its descriptor inside libroot, where --wrap=close does
// not reach. fontconfig walks the font directories right about when fd 0
// disappears.
int __wrap_closedir(DIR* dir) {
  const void* ra = __builtin_return_address(0);
  int fd = dir != nullptr ? dirfd(dir) : -1;
  if (fd >= 0 && fd <= 2) {
    fprintf(stderr, "[RCH %+.3f] closedir(fd=%d) from 0x%lx\n", Now(), fd,
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  int ret = __real_closedir(dir);
  NoteStdinIfItWentAway("closedir()", ra);
  return ret;
}

int __wrap_dup(int fd) {
  const void* ra = __builtin_return_address(0);
  int ret = __real_dup(fd);
  if (ret >= 0 && ret <= 2) {
    fprintf(stderr, "[RCH %+.3f] dup(%d) gave %d from 0x%lx\n", Now(), fd, ret,
            (unsigned long)((addr_t)ra - ImageBase()));
  }
  return ret;
}

// The resolver. It opens and closes sockets inside libnetwork, where the
// wrapped close() cannot see them, and the first name lookup lands at about
// the same moment fd 0 disappears.
int __wrap_getaddrinfo(const char* node, const char* service,
                       const struct addrinfo* hints, struct addrinfo** res) {
  const bool before = fcntl(0, F_GETFD) != -1;
  int ret = __real_getaddrinfo(node, service, hints, res);
  const bool after = fcntl(0, F_GETFD) != -1;
  if (before && !after) {
    fprintf(stderr,
            "[RCH %+.3f] getaddrinfo(\"%s\") CLOSED fd 0 -- it was open going "
            "in and gone coming out\n",
            Now(), node != nullptr ? node : "(null)");
  }
  return ret;
}

// The decisive one. Everything so far says fd 0 is still standard input
// one millisecond before socket() hands back 0, and no close() runs in
// between -- but the check was made after the call, which cannot tell
// "the descriptor was free" from "the descriptor was taken while in use".
// So look before.
int __wrap_socket(int domain, int type, int protocol) {
  struct stat before;
  const bool had = fstat(0, &before) == 0;
  const bool was_the_one_from_load = had && g_stdin_identity_known &&
                                     before.st_dev == g_stdin_dev &&
                                     before.st_ino == g_stdin_ino;
  int ret = __real_socket(domain, type, protocol);
  if (ret >= 0 && ret <= 2) {
    fprintf(stderr,
            "[RCH %+.3f] socket() returned %d; fd 0 just before the call was "
            "%s%s\n",
            Now(), ret,
            had ? "open" : "closed",
            was_the_one_from_load ? " AND STILL THE ONE FROM LOAD" : "");
  }
  return ret;
}

}  // extern "C"
