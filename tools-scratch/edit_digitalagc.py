import io

p = ("/boot/home/rchromium-chromium87-fast/chromium/third_party/webrtc/"
     "modules/audio_processing/agc/legacy/digital_agc.cc")
lines = io.open(p, encoding="utf-8").read().split("\n")

# 1-based line numbers reported by the compiler for the dead copy. The
# identically named variable further down (declared near line 494) is real:
# it is read at lines 515 and 534, so only this first one may go.
decl, set_a, set_b = 297, 302, 305

if lines[decl - 1].strip() != "int16_t L2;  // samples/subframe":
    print("UNEXPECTED_DECL: %r" % lines[decl - 1])
    raise SystemExit(1)
if lines[set_a - 1].strip() != "L2 = 3;":
    print("UNEXPECTED_SET_A: %r" % lines[set_a - 1])
    raise SystemExit(1)
if lines[set_b - 1].strip() != "L2 = 4;":
    print("UNEXPECTED_SET_B: %r" % lines[set_b - 1])
    raise SystemExit(1)

# Confirm the second declaration really is still there and untouched.
tail = "\n".join(lines[decl:])
if "int16_t L2;  // samples/subframe" not in tail:
    print("SECOND_DECL_MISSING")
    raise SystemExit(1)

for idx in sorted([decl, set_a, set_b], reverse=True):
    del lines[idx - 1]

io.open(p, "w", encoding="utf-8").write("\n".join(lines))
print("EDIT_OK")
