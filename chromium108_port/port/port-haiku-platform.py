#!/usr/bin/env python3
"""Haiku substitutes for calls that do not exist here.

Everything in build-config land is about telling Chromium which OS it is
on. This file is the other half: the places where, having been told, the
code reaches for a function Haiku does not have and something has to be put
in its place.
"""
import sys

root = sys.argv[1]

# Haiku's answer to "where does this thread's stack end", in both places that
# ask. pthread_getattr_np is a glibc extension; Haiku does not have it, and
# has never needed to, because get_thread_info() has carried stack_base and
# stack_end since BeOS. stack_end is the high address, which is what both
# callers want: they compute base + size to get there.
HAIKU_STACK_TOP = '''#elif defined(__HAIKU__)

void* GetStackTop() {
  thread_info info;
  if (get_thread_info(find_thread(nullptr), &info) != B_OK)
    return nullptr;
  return info.stack_end;
}

'''

edits = [
    # partition_alloc's own bug, surfaced by this libstdc++ rather than
    # caused by it. MetadataAllocator::operator== is not const, and the COW
    # std::string in Haiku's gcc 13 compares allocators as
    # `__a == _Alloc()` -- a const lvalue against a temporary. libc++ and the
    # SSO libstdc++ never make that comparison, so upstream never saw it. An
    # allocator's equality operator being const is what the standard asks
    # for anyway.
    ("base/allocator/partition_allocator/starscan/metadata_allocator.h",
     "  bool operator==(const MetadataAllocator<U>&) {",
     "  bool operator==(const MetadataAllocator<U>&) const {"),

    ("base/allocator/partition_allocator/starscan/stack/stack.cc",
     "#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)\n\nvoid* GetStackTop() {",
     HAIKU_STACK_TOP +
     "#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)\n\nvoid* GetStackTop() {"),

    # V8 asks the same question through its own function. Rather than take
    # Haiku out of the guard -- which would leave Stack::GetStackStart
    # undefined at link time -- the Haiku answer goes in at the top of the
    # body and the rest stays where it is.
    ("v8/src/base/platform/platform-posix.cc",
     "Stack::StackSlot Stack::GetStackStart() {\n"
     "  pthread_attr_t attr;\n"
     "  int error = pthread_getattr_np(pthread_self(), &attr);",
     "Stack::StackSlot Stack::GetStackStart() {\n"
     "#if defined(V8_OS_HAIKU)\n"
     "  thread_info info;\n"
     "  if (get_thread_info(find_thread(nullptr), &info) != B_OK)\n"
     "    return nullptr;\n"
     "  return info.stack_end;\n"
     "#else\n"
     "  pthread_attr_t attr;\n"
     "  int error = pthread_getattr_np(pthread_self(), &attr);"),
    ("v8/src/base/platform/platform-posix.cc",
     "#else\n"
     "  return nullptr;\n"
     "#endif  // !defined(V8_LIBC_GLIBC)\n"
     "}\n"
     "\n"
     "#endif  // !defined(V8_OS_FREEBSD) && !defined(V8_OS_DARWIN) &&",
     "#else\n"
     "  return nullptr;\n"
     "#endif  // !defined(V8_LIBC_GLIBC)\n"
     "#endif  // !defined(V8_OS_HAIKU)\n"
     "}\n"
     "\n"
     "#endif  // !defined(V8_OS_FREEBSD) && !defined(V8_OS_DARWIN) &&"),
]

includes = [
    # <OS.h> is where find_thread, get_thread_info and thread_info live.
    ("base/allocator/partition_allocator/starscan/stack/stack.cc",
     '#include "base/allocator/partition_allocator/starscan/stack/stack.h"'),
    ("v8/src/base/platform/platform-posix.cc",
     '#include "src/base/platform/platform-posix.h"'),
]

done = 0
for rel, old, new in edits:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing: %s" % rel)
        continue
    if new in old:
        already = old not in s
    else:
        already = new in s
    if already:
        continue
    if old not in s:
        print("  PATTERN NOT FOUND: %s" % rel)
        continue
    open(path, "w").write(s.replace(old, new, 1))
    print("  patched %s" % rel)
    done += 1

for rel, anchor in includes:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        continue
    if "#include <OS.h>" in s:
        continue
    if anchor not in s:
        print("  ANCHOR NOT FOUND: %s" % rel)
        continue
    s = s.replace(anchor,
                  anchor + "\n\n#if defined(__HAIKU__)\n#include <OS.h>\n#endif",
                  1)
    open(path, "w").write(s)
    print("  OS.h in %s" % rel)
    done += 1

print("haiku platform: %d" % done)
