import io
import os
import re
import sys

out = "/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native"
src = "/boot/home/rchromium-chromium87-fast/chromium"

pat = re.compile(r"\.\./\.\./([A-Za-z0-9_./+-]+\.(?:cc|c|cpp|h|mm|m|proto|py|json5|gni|gn))")

missing = set()
seen = set()
files = 0

for root, dirs, names in os.walk(out):
    for n in names:
        if not n.endswith(".ninja"):
            continue
        files += 1
        p = os.path.join(root, n)
        try:
            data = io.open(p, encoding="utf-8", errors="replace").read()
        except Exception:
            continue
        for m in pat.finditer(data):
            rel = m.group(1)
            if rel in seen:
                continue
            seen.add(rel)
            if not os.path.exists(os.path.join(src, rel)):
                missing.add(rel)

print("NINJA_FILES=%d" % files)
print("REFERENCED=%d" % len(seen))
print("MISSING=%d" % len(missing))

groups = {}
for rel in missing:
    top = "/".join(rel.split("/")[:3])
    groups[top] = groups.get(top, 0) + 1

for k in sorted(groups, key=lambda x: -groups[x])[:40]:
    print("%6d  %s" % (groups[k], k))

io.open("/boot/home/rchromium-native/missing-sources.txt", "w",
        encoding="utf-8").write("\n".join(sorted(missing)) + "\n")
