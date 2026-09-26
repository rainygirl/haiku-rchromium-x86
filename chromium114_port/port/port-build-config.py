#!/usr/bin/env python3
"""Add OS_HAIKU to Chromium 108's platform detection.

build/build_config.h turns a compiler predefine into the OS_* macros that
every BUILDFLAG(IS_*) reads, so nothing else in the port can be written until
Haiku exists here.

Written against 108.0.5359.124. The arm64 port has the same three edits
against trunk; the OS detection chain is identical there, the POSIX list is
wrapped differently, so this is not a copy.
"""
import sys

path = sys.argv[1]
s = open(path).read()
changed = []

# 1. Recognise the platform. __HAIKU__ is Haiku's own predefine.
old = """#elif defined(__MVS__)
#define OS_ZOS 1
#else
#error Please add support for your platform in build/build_config.h
#endif"""
new = """#elif defined(__MVS__)
#define OS_ZOS 1
#elif defined(__HAIKU__)
#define OS_HAIKU 1
#else
#error Please add support for your platform in build/build_config.h
#endif"""
if "OS_HAIKU 1" not in s:
    assert s.count(old) == 1, "OS detection chain is not as expected"
    s = s.replace(old, new)
    changed.append("OS detection")

# 2. Haiku is POSIX enough to be in the set. It has pthreads, mmap, unix
#    sockets and dlopen; what it lacks -- /proc, a fork+exec sandbox -- is
#    handled at those sites rather than by keeping Haiku out of a list that
#    roughly 500 branches test.
old = """    defined(OS_SOLARIS) || defined(OS_ZOS)
#define OS_POSIX 1
#endif"""
new = """    defined(OS_SOLARIS) || defined(OS_ZOS) || defined(OS_HAIKU)
#define OS_POSIX 1
#endif"""
if "defined(OS_HAIKU)\n#define OS_POSIX" not in s:
    assert s.count(old) == 1, "OS_POSIX list is not as expected"
    s = s.replace(old, new)
    changed.append("OS_POSIX")

# 3. The BUILDFLAG machinery needs a definition for every OS, both ways round:
#    IS_HAIKU must exist and be 0 elsewhere, or every BUILDFLAG(IS_HAIKU) is a
#    compile error rather than a false.
old = """// OS build flags
#if defined(OS_AIX)
#define BUILDFLAG_INTERNAL_IS_AIX() (1)"""
new = """// OS build flags
#if defined(OS_HAIKU)
#define BUILDFLAG_INTERNAL_IS_HAIKU() (1)
#else
#define BUILDFLAG_INTERNAL_IS_HAIKU() (0)
#endif

#if defined(OS_AIX)
#define BUILDFLAG_INTERNAL_IS_AIX() (1)"""
if "BUILDFLAG_INTERNAL_IS_HAIKU" not in s:
    assert s.count(old) == 1, "OS build flag block is not as expected"
    s = s.replace(old, new)
    changed.append("IS_HAIKU buildflag")

if changed:
    open(path, "w").write(s)
    print("build_config.h: " + ", ".join(changed))
else:
    print("build_config.h: already done")
