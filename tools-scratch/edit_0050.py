import os, shutil
ROOT = "/boot/home/rchromium-chromium87-fast/chromium"
BAK  = "/boot/home/rchromium-native/patchwork"
os.makedirs(BAK, exist_ok=True)

rel = "v8/BUILD.gn"
p = os.path.join(ROOT, rel)
b = os.path.join(BAK, "v8_BUILD.gn.orig")
if not os.path.exists(b):
    shutil.copyfile(p, b)
s = open(b).read()

old = """v8_source_set("torque_generated_initializers") {
  visibility = [ ":*" ]  # Only targets in this file can depend on this.
"""
new = """v8_source_set("torque_generated_initializers") {
  visibility = [ ":*" ]  # Only targets in this file can depend on this.

  # Jumbo merges this target's Torque output into translation units far larger
  # than a 32-bit compiler can hold: 2.8 MB, 4.5 MB and two over 1 MB, against
  # a largest-that-ever-linked-here of 925 KB. On Haiku x86 the 2.8 MB one sat
  # at zero bytes for four hours and took the machine down with it, twice.
  # Torque output is already one generated file per .tq, so nothing is gained
  # by merging it.
  never_build_jumbo = is_haiku
"""
assert s.count(old) == 1, "anchor count %d" % s.count(old)
open(os.path.join(BAK, "v8_BUILD.gn.new"), "w").write(s.replace(old, new))
print("prepared v8/BUILD.gn edit")
