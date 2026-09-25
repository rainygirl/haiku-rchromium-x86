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
if 'include_dirs = [ "haiku" ]' not in s:
    old = '''  } else if (is_linux || is_chromeos) {
    sources += [
      "epoll.c",
      "linux/config.h",
      "linux/event-config.h",
    ]
    include_dirs = [ "linux" ]'''
    new = '''  } else if (is_haiku) {
    # No epoll on Haiku; poll(2) is the backend.
    sources += [
      "haiku/config.h",
      "haiku/event-config.h",
      "poll.c",
    ]
    include_dirs = [ "haiku" ]
  } else if (is_linux || is_chromeos) {
    sources += [
      "epoll.c",
      "linux/config.h",
      "linux/event-config.h",
    ]
    include_dirs = [ "linux" ]'''
    assert old in s, "libevent BUILD.gn does not look as expected"
    open(build, "w").write(s.replace(old, new, 1))
    print("  patched third_party/libevent/BUILD.gn")
