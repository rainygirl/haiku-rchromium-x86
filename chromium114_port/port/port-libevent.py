#!/usr/bin/env python3
"""Give libevent a Haiku configuration.

libevent ships one generated config per OS -- config.h and event-config.h in
linux/, mac/, android/ and so on -- and BUILD.gn picks the directory. There is
no haiku/, so 32 translation units failed on "config.h: No such file or
directory".

Haiku's is derived from the Linux one with the Linux-only backends removed:
no epoll, no eventfd, no sys/epoll.h. poll(2) and select(2) are what is left,
and libevent's poll backend is what Haiku gets.
"""
import os
import re
import sys

root = sys.argv[1]
lib = os.path.join(root, "third_party/libevent")
src = os.path.join(lib, "linux")
dst = os.path.join(lib, "haiku")

if not os.path.isdir(src):
    print("  no linux config to derive from")
    sys.exit(1)

os.makedirs(dst, exist_ok=True)

# Things Linux has and Haiku does not.
linux_only = [
    "HAVE_EPOLL", "HAVE_EPOLL_CTL", "HAVE_SYS_EPOLL_H", "HAVE_EVENTFD",
    "HAVE_SYS_EVENTFD_H", "HAVE_SENDFILE", "HAVE_SPLICE", "HAVE_SYS_SENDFILE_H",
    "HAVE_CLOCK_GETTIME_MONOTONIC_RAW", "HAVE_SYS_PRCTL_H", "HAVE_PRCTL",
    "HAVE_SYS_SIGNALFD_H", "HAVE_SIGNALFD", "HAVE_TIMERFD_CREATE",
    "HAVE_SYS_TIMERFD_H", "HAVE_ACCEPT4", "HAVE_PIPE2", "HAVE_MALLOC_H",
]

for name in ("config.h", "event-config.h"):
    text = open(os.path.join(src, name)).read()
    out = []
    for line in text.split("\n"):
        m = re.match(r"#define (?:_EVENT_)?(\w+)", line.strip())
        if m and m.group(1).replace("_EVENT_", "") in linux_only:
            out.append("/* %s -- not on Haiku */" % line.strip())
            continue
        out.append(line)
    open(os.path.join(dst, name), "w").write("\n".join(out))
    print("  wrote third_party/libevent/haiku/%s" % name)

# And teach BUILD.gn to use it. Haiku takes the poll backend.
build = os.path.join(lib, "BUILD.gn")
s = open(build).read()
if 'include_dirs += [ "haiku" ]' not in s:
    old = '''  } else if (is_linux || is_chromeos) {
    sources += [
      "epoll.c",
      "linux/config.h",
      "linux/event-config.h",
    ]
    include_dirs += [ "linux" ]'''
    new = '''  } else if (is_haiku) {
    # No epoll on Haiku. poll.c is already in the common source list above,
    # so adding it here makes gn refuse the target: "generates two object
    # files with the same name".
    sources += [
      "haiku/config.h",
      "haiku/event-config.h",
    ]
    include_dirs += [ "haiku" ]
  } else if (is_linux || is_chromeos) {
    sources += [
      "epoll.c",
      "linux/config.h",
      "linux/event-config.h",
    ]
    include_dirs += [ "linux" ]'''
    assert old in s, "libevent BUILD.gn does not look as expected"
    open(build, "w").write(s.replace(old, new, 1))
    print("  patched third_party/libevent/BUILD.gn")

# And the Chromium-specific dispatcher, which is a separate file from the
# per-OS ones and has its own chain ending in #error.
disp = os.path.join(lib, "event-config.h")
t = open(disp).read()
if "haiku/event-config.h" not in t:
    old = ('#elif defined(__linux__)\n'
           '#include "third_party/libevent/linux/event-config.h"')
    new = ('#elif defined(__HAIKU__)\n'
           '#include "third_party/libevent/haiku/event-config.h"\n'
           '#elif defined(__linux__)\n'
           '#include "third_party/libevent/linux/event-config.h"')
    assert old in t, "libevent event-config.h dispatcher is not as expected"
    open(disp, "w").write(t.replace(old, new, 1))
    print("  patched third_party/libevent/event-config.h")

# poll() on Haiku reports POLLNVAL whether or not it was asked for, and
# libevent's poll backend keeps slots nothing watches any more. A slot whose
# descriptor has been closed then comes back POLLNVAL on every call, poll()
# never blocks again, and NetworkService spins in the kernel for the life of
# the process. Measured on 114 (2026-09-30): 48% of the VAIO with YouTube idle,
# 13 dead slots, thousands of calls a second. The same fix as the 87 port's
# patch 0091; see its header for the whole story.
poll_c = os.path.join(lib, "poll.c")
p = open(poll_c).read()
if "Drop slots nothing is watching" not in p:
    old = ("\tnfds = pop->nfds;\n"
           "\tres = poll(pop->event_set, nfds, msec);\n")
    new = ("#if defined(__HAIKU__)\n"
           "\t/* Drop slots nothing is watching any more. Haiku's poll() ORs\n"
           "\t * POLLERR|POLLHUP|POLLNVAL into events, so a slot is live only\n"
           "\t * if it asks for POLLIN or POLLOUT. poll_add() builds a fresh\n"
           "\t * slot if the descriptor comes back. */\n"
           "\tfor (i = pop->nfds - 1; i >= 0; --i) {\n"
           "\t\tstruct pollfd *dead = &pop->event_set[i];\n"
           "\t\tif (dead->events & (POLLIN|POLLOUT))\n"
           "\t\t\tcontinue;\n"
           "\t\tpop->idxplus1_by_fd[dead->fd] = 0;\n"
           "\t\t--pop->nfds;\n"
           "\t\tif (i != pop->nfds) {\n"
           "\t\t\tmemcpy(&pop->event_set[i], &pop->event_set[pop->nfds],\n"
           "\t\t\t       sizeof(struct pollfd));\n"
           "\t\t\tpop->event_r_back[i] = pop->event_r_back[pop->nfds];\n"
           "\t\t\tpop->event_w_back[i] = pop->event_w_back[pop->nfds];\n"
           "\t\t\tpop->idxplus1_by_fd[pop->event_set[i].fd] = i + 1;\n"
           "\t\t}\n"
           "\t}\n"
           "#endif\n\n" + old)
    assert p.count(old) == 1, "libevent poll.c dispatch is not as expected"
    p = p.replace(old, new, 1)
    old2 = ("\t\tif (what & (POLLHUP|POLLERR))\n"
            "\t\t\twhat |= POLLIN|POLLOUT;\n")
    new2 = ("#if defined(__HAIKU__)\n"
            "\t\t/* A watched descriptor that is already gone reaches its\n"
            "\t\t * owner too, which then closes and unregisters it. */\n"
            "\t\tif (what & (POLLHUP|POLLERR|POLLNVAL))\n"
            "\t\t\twhat |= POLLIN|POLLOUT;\n"
            "#else\n" + old2 + "#endif\n")
    assert p.count(old2) == 1, "libevent poll.c revents handling is not as expected"
    p = p.replace(old2, new2, 1)
    open(poll_c, "w").write(p)
    print("  patched third_party/libevent/poll.c (dead descriptors)")
