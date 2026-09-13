import os, shutil

ROOT = "/boot/home/rchromium-chromium87-fast/chromium"
BAK  = "/boot/home/rchromium-native/patchwork"
os.makedirs(BAK, exist_ok=True)

FILES = [
    "cc/trees/render_frame_metadata.h",
    "cc/trees/render_frame_metadata.cc",
    "cc/mojom/render_frame_metadata_mojom_traits.h",
    "cc/mojom/render_frame_metadata_mojom_traits.cc",
    "cc/trees/layer_tree_host_impl.cc",
]

OLD = "#if defined(OS_ANDROID) || defined(TOOLKIT_QT)"
NEW = "#if defined(OS_ANDROID) || defined(TOOLKIT_QT) || defined(OS_HAIKU)"

for rel in FILES:
    p = os.path.join(ROOT, rel)
    base = rel.replace("/", "_")
    b = os.path.join(BAK, base + ".orig")
    if not os.path.exists(b):
        shutil.copyfile(p, b)
    s = open(b).read()
    n = s.count(OLD)
    assert n >= 1, "%s: no anchor" % rel
    open(os.path.join(BAK, base + ".new"), "w").write(s.replace(OLD, NEW))
    print("%-50s %d site(s)" % (rel, n))
print("OK")
