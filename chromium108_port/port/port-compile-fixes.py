#!/usr/bin/env python3
"""Source fixes needed to compile Chromium 108, collected as they surfaced.

Not all of these are about Haiku. The first is a plain missing include that
only shows up with gcc 12+, which is newer than what 108 was built with.
"""
import sys

root = sys.argv[1]
edits = [
    # Enterprise policy keys are generated only for the platforms each policy
    # declares in its supported_on list, and "haiku" appears in none of them,
    # so policy::key::kProxySettings and forty others were never emitted.
    # The script already remaps one target_os to a template name; Haiku gets
    # the same treatment and reads Linux's set. That is the right set: a
    # policy on Haiku would be the same JSON file in the same place, and
    # nothing here is Linux-kernel-specific.
    ("components/policy/tools/generate_policy_source.py",
     "  if target_platform == 'chromeos':\n"
     "    target_platform = 'chrome_os'",
     "  if target_platform == 'chromeos':\n"
     "    target_platform = 'chrome_os'\n"
     "  # Haiku is not a platform the templates know about. It reads Linux's\n"
     "  # policies: same file, same location, nothing kernel-specific.\n"
     "  if target_platform == 'haiku':\n"
     "    target_platform = 'linux'"),
    # quiche uses int64_t in a header that never includes <cstdint>; it
    # arrived transitively on glibc and does not here.
    ("net/third_party/quiche/src/quiche/http2/adapter/window_manager.h",
     "#include <functional>",
     "#include <cstdint>\n#include <functional>"),
    # Not a Haiku problem, a gcc-versus-clang one, and it lands on the host
    # toolchain as much as the target. A constructor cannot be named with an
    # explicit template argument list: inside the class,
    # CheckedThreadLocalOwnedPointer<T> is the injected-class-name, and using
    # it with <T> makes the declaration a function returning that type rather
    # than a constructor. clang accepts it; gcc says "expected unqualified-id
    # before 'const'", 240 times across the two deleted members.
    ("base/threading/thread_local_internal.h",
     "  CheckedThreadLocalOwnedPointer<T>(const CheckedThreadLocalOwnedPointer<T>&) =\n"
     "      delete;",
     "  CheckedThreadLocalOwnedPointer(const CheckedThreadLocalOwnedPointer<T>&) =\n"
     "      delete;"),
    # partition_alloc_forward.h uses uintptr_t without including <cstdint>.
    # 108 was built with clang, whose libc++ pulls it in transitively; gcc 12
    # does not, and the error lands on the host x64 build before Haiku is
    # even reached.
    ("base/allocator/partition_allocator/partition_alloc_forward.h",
     "#include <cstddef>",
     "#include <cstddef>\n#include <cstdint>"),

    # fieldtrial_to_struct.py validates --platform against a fixed list.
    # content_shell reads the generated config but does not act on it, so
    # Haiku only needs to be an accepted name.
    ("tools/variations/fieldtrial_to_struct.py",
     "    'fuchsia',\n",
     "    'fuchsia',\n    'haiku',\n"),
]

done = 0
for rel, old, new in edits:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing file: %s" % rel)
        continue
    if new in s:
        continue
    if old not in s:
        print("  PATTERN NOT FOUND: %s" % rel)
        continue
    open(path, "w").write(s.replace(old, new, 1))
    print("  patched %s" % rel)
    done += 1
print("compile fixes: %d" % done)
