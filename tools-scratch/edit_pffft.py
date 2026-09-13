import io

p = "/boot/home/rchromium-chromium87-fast/chromium/third_party/pffft/src/pffft.c"
s = io.open(p, encoding="utf-8").read()

if "RCHROMIUM_UNUSED" in s:
    print("ALREADY_PATCHED")
    raise SystemExit(0)

old = "  v4sf_union a0, a1, a2, a3, t, u; \n"
new = ("  /* RCHROMIUM_UNUSED: t and u are read only by assertv4(), which\n"
       "     compiles away under NDEBUG, so GCC reports them as set but\n"
       "     unused in a release build. */\n"
       "  v4sf_union a0, a1, a2, a3;\n"
       "  v4sf_union t __attribute__((unused));\n"
       "  v4sf_union u __attribute__((unused));\n")
assert s.count(old) == 1, "decl count=%d" % s.count(old)
s = s.replace(old, new, 1)

io.open(p, "w", encoding="utf-8").write(s)
print("EDIT_OK")
