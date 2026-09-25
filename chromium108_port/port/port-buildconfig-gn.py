#!/usr/bin/env python3
"""Make GN recognise haiku as a target OS in Chromium 108.

Three things are needed before `gn gen` will do anything: a toolchain to
default to, an `is_haiku` for the ~500 GN conditionals to read, and
membership in `is_posix`. The last one is subtle -- 108 defines is_posix as
"not win and not fuchsia", so Haiku is already inside it and must not be
added again, unlike build_config.h where the list is explicit.
"""
import sys

path = sys.argv[1]
s = open(path).read()
changed = []

# 1. A default toolchain. Haiku's gcc is not clang, so it takes the same shape
#    as the aix and zos entries rather than the linux one.
old = """} else if (target_os == "zos") {
  _default_toolchain = "//build/toolchain/zos:$target_cpu"
} else {"""
new = """} else if (target_os == "zos") {
  _default_toolchain = "//build/toolchain/zos:$target_cpu"
} else if (target_os == "haiku") {
  _default_toolchain = "//build/toolchain/haiku:$target_cpu"
} else {"""
if 'target_os == "haiku"' not in s:
    assert s.count(old) == 1, "default toolchain chain is not as expected"
    s = s.replace(old, new)
    changed.append("default toolchain")

# 2. is_haiku, next to the other is_* definitions. Note what is NOT added:
#    is_posix is computed as !is_win && !is_fuchsia in 108, so Haiku is
#    already posix and saying so again would be wrong, not merely redundant.
old = """is_win = current_os == "win" || current_os == "winuwp\""""
new = """is_haiku = current_os == "haiku"
is_win = current_os == "win" || current_os == "winuwp\""""
if "is_haiku = " not in s:
    assert s.count(old) == 1, "is_* block is not as expected"
    s = s.replace(old, new)
    changed.append("is_haiku")

if changed:
    open(path, "w").write(s)
    print("BUILDCONFIG.gn: " + ", ".join(changed))
else:
    print("BUILDCONFIG.gn: already done")
