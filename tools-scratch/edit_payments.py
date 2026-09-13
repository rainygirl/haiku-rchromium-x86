import io

p = "/boot/home/rchromium-chromium87-fast/chromium/components/payments/core/BUILD.gn"
lines = io.open(p, encoding="utf-8").read().split("\n")

if any("HAIKU_PAYMENTS_CORE_SUBSET" in l for l in lines):
    print("ALREADY_PATCHED")
    raise SystemExit(0)

start = None
for i, l in enumerate(lines):
    if l.startswith('jumbo_static_library("core") {'):
        start = i
        break
assert start is not None, "core target not found"

end = None
for i in range(start + 1, len(lines)):
    if lines[i] == "}":
        end = i
        break
assert end is not None, "core target end not found"

body = lines[start:end + 1]

head = [
    "# HAIKU_PAYMENTS_CORE_SUBSET",
    "# content/browser is the only consumer of this target in a content_shell",
    "# build, and it uses PaymentsValidators alone. The full target reaches",
    "# //components/autofill/core/browser, which in turn requires",
    "# //components/sync, //components/policy, //components/translate,",
    "# //components/invalidation, //components/gcm_driver, libphonenumber and",
    "# libaddressinput. The pinned qtwebengine-chromium distribution ships those",
    "# BUILD.gn files without their sources, so the full target cannot be built",
    "# here. payments_validators.cc needs nothing beyond base, re2 and url.",
    "if (is_haiku) {",
    '  jumbo_static_library("core") {',
    "    sources = [",
    '      "payments_validators.cc",',
    '      "payments_validators.h",',
    "    ]",
    "",
    "    deps = [",
    '      "//base",',
    '      "//third_party/re2",',
    '      "//url",',
    "    ]",
    "  }",
    "} else {",
]
indented = ["  " + l if l.strip() else l for l in body]
tail = ["}"]

lines[start:end + 1] = head + indented + tail
io.open(p, "w", encoding="utf-8").write("\n".join(lines))
print("EDIT_OK")
