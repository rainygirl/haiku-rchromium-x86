import os, shutil
ROOT = "/boot/home/rchromium-chromium87-fast/chromium"
BAK  = "/boot/home/rchromium-native/patchwork"
p = os.path.join(ROOT, "v8/BUILD.gn")
b = os.path.join(BAK, "v8_BUILD.gn.after0051")
if not os.path.exists(b):
    shutil.copyfile(p, b)
s = open(b).read()

target = 'v8_source_set("v8_base_without_compiler") {'
assert s.count(target) == 1, "anchor count %d" % s.count(target)

comment = """
  # Fourth V8 target to exceed what a 32-bit compiler can hold once jumbo has
  # merged it: four of its ten units are 1.0, 1.6, 1.8 and 1.8 MB, against the
  # 925 KB that is the largest unit observed to compile on this machine. Its
  # generated units only appeared after 0050 and 0051 changed the graph, which
  # is why it is a separate patch.
  never_build_jumbo = is_haiku
"""
open(os.path.join(BAK, "v8_BUILD.gn.new3"), "w").write(s.replace(target, target + comment))
print("prepared v8_base_without_compiler edit")
