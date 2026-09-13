import os, shutil
ROOT = "/boot/home/rchromium-chromium87-fast/chromium"
BAK  = "/boot/home/rchromium-native/patchwork"
p = os.path.join(ROOT, "v8/BUILD.gn")
b = os.path.join(BAK, "v8_BUILD.gn.after0050")
if not os.path.exists(b):
    shutil.copyfile(p, b)
s = open(b).read()

COMMENT = """
  # Same 32-bit jumbo limit as torque_generated_initializers: this target's
  # merged units are %s, well past the 925 KB that is the largest one
  # observed to compile here. Measured, not guessed -- gl_jumbo_1 at 911 KB and
  # cc_jumbo_4 at 925 KB both build, and everything above that in this tree is
  # V8.
  never_build_jumbo = is_haiku
"""

for target, sizes in [
    ('v8_source_set("v8_initializers") {', "1.3 MB"),
    ('v8_source_set("v8_compiler") {', "1.0, 1.5 and 2.0 MB"),
]:
    assert s.count(target) == 1, "anchor %r count %d" % (target, s.count(target))
    s = s.replace(target, target + (COMMENT % sizes))

open(os.path.join(BAK, "v8_BUILD.gn.new2"), "w").write(s)
print("prepared v8_initializers + v8_compiler edits")
