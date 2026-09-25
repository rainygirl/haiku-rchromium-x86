#!/usr/bin/env python3
"""Teach V8 and perfetto that Haiku exists.

Both keep their own OS-detection chain, separate from build/build_config.h,
and both stop at the end of it. Between them they accounted for 8,287 of the
errors in the first clean compile -- V8's mutex.h alone produced 7,587
"'NativeHandle' does not name a type", because without V8_OS_POSIX none of the
arms defining it are taken.

Haiku gets Linux's answers. It has pthreads, and where it differs -- no /proc,
no fork+exec sandbox -- that is handled at the call sites rather than by
pretending it is not POSIX.
"""
import sys

root = sys.argv[1]
edits = [
    # Skia keeps a third OS chain, and its last arm is not an #error but
    # "#define SK_BUILD_FOR_MAC" -- so an unrecognised system silently
    # becomes a Mac. That is how SkSemaphore.cpp came to ask for
    # <dispatch/dispatch.h>. Haiku belongs in the unix arm: pthreads,
    # POSIX semaphores, the lot.
    ("third_party/skia/include/core/SkTypes.h",
     "          defined(__GLIBC__) || defined(__GNU__) || defined(__unix__)\n"
     "        #define SK_BUILD_FOR_UNIX",
     "          defined(__GLIBC__) || defined(__GNU__) || defined(__unix__) || \\\n"
     "          defined(__HAIKU__)\n"
     "        #define SK_BUILD_FOR_UNIX"),

    # V8: v8config.h is the root of every V8_OS_* decision.
    ("v8/include/v8config.h",
     "#elif defined(__linux__)\n"
     "# define V8_OS_LINUX 1\n"
     "# define V8_OS_POSIX 1\n"
     '# define V8_OS_STRING "linux"',
     "#elif defined(__HAIKU__)\n"
     "# define V8_OS_HAIKU 1\n"
     "# define V8_OS_POSIX 1\n"
     '# define V8_OS_STRING "haiku"\n'
     "\n"
     "#elif defined(__linux__)\n"
     "# define V8_OS_LINUX 1\n"
     "# define V8_OS_POSIX 1\n"
     '# define V8_OS_STRING "linux"'),

    # perfetto: its chain ends in #error, and every PERFETTO_BUILDFLAG(...)
    # then fails to expand, which is where the 633 "missing binary operator
    # before token '('" came from.
    #
    # Every OS is 0, Haiku's own included, and that is deliberate. Claiming
    # PERFETTO_OS_LINUX was the obvious first move and the wrong one: it is
    # not a stand-in for "POSIX" in this codebase, it is the flag guarding
    # eventfd, timerfd and syscall(SYS_gettid), and Haiku has none of the
    # three. Perfetto already writes its fallbacks as "Mac, Fuchsia and other
    # non-Linux UNIXes", so leaving every flag at 0 lands in arms that use
    # pipe(), pthread_self() and a plain timer -- which is what Haiku can do.
    ("third_party/perfetto/include/perfetto/base/build_config.h",
     "#else\n#error OS not supported (see build_config.h)\n#endif",
     "#elif defined(__HAIKU__)\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0\n"
     "#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0\n"
     "#else\n#error OS not supported (see build_config.h)\n#endif"),

    # Adding V8_OS_HAIKU opens holes wherever V8 asks "posix but not X".
    # Haiku's libroot has no malloc_usable_size, so it belongs with AIX on
    # the exclusion side -- 578 errors came from this one line.
    ("v8/src/base/platform/memory.h",
     "#if (V8_OS_POSIX && !V8_OS_AIX) || V8_OS_WIN\n"
     "#define V8_HAS_MALLOC_USABLE_SIZE 1",
     "#if (V8_OS_POSIX && !V8_OS_AIX && !V8_OS_HAIKU) || V8_OS_WIN\n"
     "#define V8_HAS_MALLOC_USABLE_SIZE 1"),

    # Not a Haiku problem: export-template.h ends with four static_asserts
    # that check its own macro machinery, and gcc 13 does not expand
    # EXPORT_TEMPLATE_STYLE the way they expect. 539 errors, all from a
    # self-test. The macros it is testing work; only the test does not.
    ("v8/src/base/export-template.h",
     "EXPORT_TEMPLATE_TEST(DEFAULT, );",
     "#if !defined(__HAIKU__)\nEXPORT_TEMPLATE_TEST(DEFAULT, );"),
    ("v8/src/base/export-template.h",
     "EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));",
     "EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));\n#endif"),
]

done = 0
for rel, old, new in edits:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing file: %s" % rel)
        continue
    # Whether an edit has already been applied cannot be asked the same way
    # for every edit, and getting it wrong is silent both ways.
    #
    #   removal (the replacement is a fragment of what it replaces): `new` is
    #   a substring of the original too, so `new in s` is true before anything
    #   is done and the edit is skipped forever.
    #
    #   addition (the replacement contains what it replaces, as perfetto's
    #   does): `old` is still there afterwards, so `old in s` is true again
    #   and the edit is applied a second and a third time.
    #
    # Which of the two an edit is can be read off the edit itself.
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
print("OS detection: %d" % done)
