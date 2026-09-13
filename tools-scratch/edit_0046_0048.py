import os, shutil, sys

ROOT = "/boot/home/rchromium-chromium87-fast/chromium"
BAK  = "/boot/home/rchromium-native/patchwork"
os.makedirs(BAK, exist_ok=True)

def load(rel):
    p = os.path.join(ROOT, rel)
    b = os.path.join(BAK, os.path.basename(rel) + ".orig")
    if not os.path.exists(b):
        shutil.copyfile(p, b)
    with open(p) as f:
        return p, f.read()

def save(p, rel, text):
    e = os.path.join(BAK, os.path.basename(rel) + ".new")
    with open(e, "w") as f:
        f.write(text)
    print("wrote", e)

# --- 1. angle: do not use libpci on Haiku -------------------------------
rel = "third_party/angle/BUILD.gn"
p, s = load(rel)
old = """  use_libpci =
      (is_linux || is_chromeos) && (!is_chromecast || is_cast_desktop_build) &&
      (angle_use_x11 || use_ozone) && angle_has_build
"""
new = """  use_libpci =
      (is_linux || is_chromeos) && (!is_chromecast || is_cast_desktop_build) &&
      (angle_use_x11 || use_ozone) && angle_has_build && !is_haiku
"""
assert s.count(old) == 1, "angle anchor count %d" % s.count(old)
save(p, rel, s.replace(old, new))

# --- 2. crashpad: Haiku already defines WCOREDUMP -----------------------
rel = "third_party/crashpad/crashpad/util/posix/double_fork_and_exec.cc"
p, s = load(rel)
old = """#if defined(OS_HAIKU)
#define WCOREDUMP(x) WIFCORED(x)
#endif
"""
new = """#if defined(OS_HAIKU) && !defined(WCOREDUMP)
#define WCOREDUMP(x) WIFCORED(x)
#endif
"""
assert s.count(old) == 1, "crashpad anchor count %d" % s.count(old)
save(p, rel, s.replace(old, new))

# --- 3. gpu: guard the Qt call site like its declaration ----------------
rel = "gpu/ipc/client/command_buffer_proxy_impl.cc"
p, s = load(rel)
old = """  bool create_async = !CreateCommandBufferSyncQt(
      init_params,
      channel->channel_id(),
      route_id_,
      &region,
      &result,
      &capabilities_);
"""
new = """#if defined(TOOLKIT_QT)
  bool create_async = !CreateCommandBufferSyncQt(
      init_params,
      channel->channel_id(),
      route_id_,
      &region,
      &result,
      &capabilities_);
#else
  bool create_async = true;
#endif
"""
assert s.count(old) == 1, "gpu anchor count %d" % s.count(old)
save(p, rel, s.replace(old, new))

print("OK")
