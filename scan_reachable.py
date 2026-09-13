import io
import json
import os

out = "/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native"
src = "/boot/home/rchromium-chromium87-fast/chromium"

data = json.load(io.open(os.path.join(out, "project.json"), encoding="utf-8"))
targets = data["targets"]
print("TOTAL_TARGETS=%d" % len(targets))

root = "//content/shell:content_shell"
reachable = set()
stack = [root]
while stack:
    t = stack.pop()
    if t in reachable or t not in targets:
        continue
    reachable.add(t)
    info = targets[t]
    for key in ("deps", "public_deps"):
        for d in info.get(key, []):
            if d not in reachable:
                stack.append(d)
print("REACHABLE_TARGETS=%d" % len(reachable))


def to_path(label):
    # //foo/bar/baz.cc -> foo/bar/baz.cc
    return label[2:] if label.startswith("//") else label


missing = {}
checked = 0
for t in reachable:
    info = targets[t]
    bad = []
    for s in info.get("sources", []):
        if not s.startswith("//"):
            continue
        rel = to_path(s)
        if rel.startswith("out/"):
            continue
        checked += 1
        if not os.path.exists(os.path.join(src, rel)):
            bad.append(rel)
    if bad:
        missing[t] = bad

print("SOURCES_CHECKED=%d" % checked)
print("TARGETS_WITH_MISSING=%d" % len(missing))
total = sum(len(v) for v in missing.values())
print("MISSING_FILES=%d" % total)
print("")
for t in sorted(missing, key=lambda x: -len(missing[x])):
    print("%5d  %s" % (len(missing[t]), t))

with io.open("/boot/home/rchromium-native/missing-reachable.txt", "w",
             encoding="utf-8") as f:
    for t in sorted(missing):
        f.write("### %s\n" % t)
        for s in missing[t]:
            f.write("%s\n" % s)
