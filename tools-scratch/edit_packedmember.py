import io

p = "/boot/home/rchromium-chromium87-fast/chromium/build/config/compiler/BUILD.gn"
s = io.open(p, encoding="utf-8").read()

if "address-of-packed-member" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

old = '''      cflags_cc += [ "-Wno-return-type" ]
      if (!is_haiku) {
        cflags_cc += [ "-Wno-deprecated-copy" ]
      }
'''
new = '''      cflags_cc += [ "-Wno-return-type" ]
      if (!is_haiku) {
        cflags_cc += [ "-Wno-deprecated-copy" ]
      }

      if (is_haiku) {
        # Haiku declares struct in6_addr _PACKED even though its union member
        # is a uint32_t array, so any code that reads an IPv6 address through
        # a wider type trips -Waddress-of-packed-member. The alignment is
        # actually fine; the attribute is what misleads GCC. Clang builds of
        # Chromium disable the same warning.
        cflags += [ "-Wno-address-of-packed-member" ]
      }
'''
assert s.count(old) == 1, "gcc block count=%d" % s.count(old)
s = s.replace(old, new, 1)

io.open(p, "w", encoding="utf-8").write(s)
print("EDIT_OK")
