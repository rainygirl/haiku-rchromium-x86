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
    # perfetto asks the OS for a thread id and, with every PERFETTO_OS_*
    # flag at 0, lands in "Default to pthreads in case no OS is set", where
    # PlatformThreadId is pthread_t. On Haiku that is a pointer, so the
    # static_cast to uint32_t that the tracing code does is ill-formed.
    # Haiku's own thread id is a small integer and find_thread(nullptr)
    # returns it.
    ("third_party/perfetto/include/perfetto/base/thread_utils.h",
     "#else\n#include <pthread.h>\n#endif",
     "#elif defined(__HAIKU__)\n#include <OS.h>\n"
     "#else\n#include <pthread.h>\n#endif"),
    ("third_party/perfetto/include/perfetto/base/thread_utils.h",
     "#else  // Default to pthreads in case no OS is set.\n"
     "using PlatformThreadId = pthread_t;",
     "#elif defined(__HAIKU__)\n"
     "using PlatformThreadId = int32_t;\n"
     "inline PlatformThreadId GetThreadId() {\n"
     "  return static_cast<int32_t>(find_thread(nullptr));\n"
     "}\n"
     "#else  // Default to pthreads in case no OS is set.\n"
     "using PlatformThreadId = pthread_t;"),

    # CLOCK_BOOTTIME is a Linux clock and Haiku does not define it. The
    # code already treats it as something that may not work -- it calls
    # clock_gettime and falls back to the wall clock if that fails -- so
    # the fallback just has to be reachable when the constant is missing
    # too. Guarding on the macro rather than on the OS keeps that local.
    ("third_party/perfetto/include/perfetto/base/time.h",
     "  static const clockid_t kBootTimeClockSource = [] {\n"
     "    struct timespec ts = {};\n"
     "    int res = clock_gettime(CLOCK_BOOTTIME, &ts);\n"
     "    return res == 0 ? CLOCK_BOOTTIME : kWallTimeClockSource;\n"
     "  }();",
     "  static const clockid_t kBootTimeClockSource = [] {\n"
     "#if defined(CLOCK_BOOTTIME)\n"
     "    struct timespec ts = {};\n"
     "    int res = clock_gettime(CLOCK_BOOTTIME, &ts);\n"
     "    return res == 0 ? CLOCK_BOOTTIME : kWallTimeClockSource;\n"
     "#else\n"
     "    return kWallTimeClockSource;\n"
     "#endif\n"
     "  }();"),

    # timegm exists on Haiku, in libbsd, declared in headers/bsd/time.h --
    # which is not on the include path any more and should not be, because
    # of the ALIGN collision that put it there. -lbsd is linked globally, so
    # the function is present; only its declaration is missing, and one
    # line supplies that.
    ("third_party/perfetto/include/perfetto/base/time.h",
     "inline int64_t TimeGm(struct tm* tms) {",
     "#if defined(__HAIKU__)\n"
     "extern \"C\" time_t timegm(struct tm*);\n"
     "#endif\n"
     "\n"
     "inline int64_t TimeGm(struct tm* tms) {"),

    # Haiku's struct dirent has no d_type -- it carries the device and inode
    # numbers instead, which is a BeOS inheritance rather than an omission.
    # stat() answers the same question at the cost of a syscall per entry,
    # and this walk is not on any hot path.
    ("third_party/perfetto/src/base/file_utils.cc",
     "      if (dirent->d_type == DT_DIR) {\n"
     "        dir_queue.push_back(cur_dir + dirent->d_name + '/');\n"
     "      } else if (dirent->d_type == DT_REG) {\n"
     "        const std::string full_path = cur_dir + dirent->d_name;\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }",
     "#if defined(__HAIKU__)\n"
     "      const std::string full_path = cur_dir + dirent->d_name;\n"
     "      struct stat entry_stat;\n"
     "      if (stat(full_path.c_str(), &entry_stat) != 0)\n"
     "        continue;\n"
     "      if (S_ISDIR(entry_stat.st_mode)) {\n"
     "        dir_queue.push_back(full_path + '/');\n"
     "      } else if (S_ISREG(entry_stat.st_mode)) {\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }\n"
     "#else\n"
     "      if (dirent->d_type == DT_DIR) {\n"
     "        dir_queue.push_back(cur_dir + dirent->d_name + '/');\n"
     "      } else if (dirent->d_type == DT_REG) {\n"
     "        const std::string full_path = cur_dir + dirent->d_name;\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }\n"
     "#endif"),

    # libphonenumber picks between a real ThreadChecker and an empty one by
    # OS name. Haiku matched neither, so it got the empty class -- and a
    # class with no members and no user-provided constructor cannot be
    # declared const, which is what "uninitialized const member" means here.
    # Haiku has pthreads; it belongs in the real one.
    ("third_party/libphonenumber/dist/cpp/src/phonenumbers/base/thread_checker.h",
     "    (defined(__linux__) || defined(__APPLE__) || defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD))",
     "    (defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__) || \\\n"
     "     defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD))"),
    # sys/syscall.h is included for gettid, unconditionally except on AIX
    # and Fuchsia. Haiku has neither the header nor the call; the Haiku arm
    # added to Stack::GetStackStart below is inside the file, which does not
    # help when the file will not preprocess.
    ("v8/src/base/platform/platform-posix.cc",
     "#if !defined(_AIX) && !defined(V8_OS_FUCHSIA)\n"
     "#include <sys/syscall.h>\n"
     "#endif",
     "#if !defined(_AIX) && !defined(V8_OS_FUCHSIA) && !defined(V8_OS_HAIKU)\n"
     "#include <sys/syscall.h>\n"
     "#endif"),
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
