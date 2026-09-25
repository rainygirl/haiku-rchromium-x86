#!/usr/bin/env python3
"""Source fixes needed to compile Chromium 108, collected as they surfaced.

Not all of these are about Haiku. The first is a plain missing include that
only shows up with gcc 12+, which is newer than what 108 was built with.
"""
import sys

root = sys.argv[1]
edits = [
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
