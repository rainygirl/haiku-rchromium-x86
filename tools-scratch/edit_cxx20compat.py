import io

p = "/boot/home/rchromium-chromium87-fast/chromium/build/config/compiler/BUILD.gn"
s = io.open(p, encoding="utf-8").read()

if "c++20-compat" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

old = '''        cflags += [ "-Wno-address-of-packed-member" ]
      }
'''
new = '''        cflags += [ "-Wno-address-of-packed-member" ]

        # GCC enables -Wc++20-compat as part of -Wall. Mojom generates struct
        # fields named "requires" (for example in InterfaceProviderSpec), which
        # is a keyword in C++20 but a plain identifier in the gnu++14 this
        # build uses. The headers are generated, so suppressing the warning is
        # the durable fix; editing the output would not survive regeneration.
        cflags_cc += [ "-Wno-c++20-compat" ]
      }
'''
assert s.count(old) == 1, "haiku block count=%d" % s.count(old)
s = s.replace(old, new, 1)

io.open(p, "w", encoding="utf-8").write(s)
print("EDIT_OK")
