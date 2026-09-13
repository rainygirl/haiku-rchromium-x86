import io

p = ("/boot/home/rchromium-chromium87-fast/chromium/third_party/webrtc/"
     "rtc_base/event_tracer.cc")
s = io.open(p, encoding="utf-8").read()

if "__HAIKU__" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

old = '''#if defined(WEBRTC_WIN)
                ", \\"tid\\": %lu"
#else
                ", \\"tid\\": %d"
#endif  // defined(WEBRTC_WIN)
'''
new = '''#if defined(WEBRTC_WIN)
                ", \\"tid\\": %lu"
#elif defined(__HAIKU__)
                // Haiku's thread_id is a long, so rtc::PlatformThreadId is
                // wider than the int that %d expects.
                ", \\"tid\\": %ld"
#else
                ", \\"tid\\": %d"
#endif  // defined(WEBRTC_WIN)
'''
assert s.count(old) == 1, "tid format block count=%d" % s.count(old)
s = s.replace(old, new, 1)

io.open(p, "w", encoding="utf-8").write(s)
print("EDIT_OK")
