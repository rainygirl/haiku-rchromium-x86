#!/usr/bin/env python3
"""A HidService for Haiku, because there being none is fatal.

`HidService::Create()` returns nullptr on a platform it does not know, and
`HidManagerImpl`'s constructor then does

	DCHECK(hid_service_);
	hid_service_observation_.Observe(hid_service_.get());

The DCHECK is compiled out of a release build and what is left is AddObserver()
on a null pointer. x.com's sign-in page asks for the HID device list -- a
passkey is a HID device -- and the browser died in
`device::HidService::AddObserver` every time, with a fault address that moved
around because the null `this` was being offset into.

Fuchsia is in the same position and answers it with a stub that finds nothing;
`files/services/device/hid/hid_service_haiku.{h,cc}` is that stub for Haiku.
This puts it in the build and in Create().
"""
import sys

root = sys.argv[1]
done = 0

path = "%s/services/device/hid/hid_service.cc" % root
s = open(path).read()
if "HidServiceHaiku" in s:
    print("  hid_service.cc: 이미 적용")
else:
    old = ("#elif BUILDFLAG(IS_FUCHSIA)\n"
           "  return std::make_unique<HidServiceFuchsia>();\n"
           "#else\n"
           "  return nullptr;\n"
           "#endif")
    assert s.count(old) == 1, s.count(old)
    new = ("#elif BUILDFLAG(IS_FUCHSIA)\n"
           "  return std::make_unique<HidServiceFuchsia>();\n"
           "#elif BUILDFLAG(IS_HAIKU)\n"
           "  return std::make_unique<HidServiceHaiku>();\n"
           "#else\n"
           "  return nullptr;\n"
           "#endif")
    s = s.replace(old, new, 1)
    anchor = '#include "services/device/hid/hid_service_fuchsia.h"'
    if anchor in s:
        s = s.replace(anchor, anchor + "\n#elif BUILDFLAG(IS_HAIKU)\n"
                      '#include "services/device/hid/hid_service_haiku.h"', 1)
    else:
        # The Fuchsia include sits under its own guard; add ours next to the
        # others rather than guessing at that guard's shape.
        anchor = '#include "services/device/hid/hid_service.h"'
        assert s.count(anchor) == 1, s.count(anchor)
        s = s.replace(anchor, anchor + "\n\n#if BUILDFLAG(IS_HAIKU)\n"
                      '#include "services/device/hid/hid_service_haiku.h"\n'
                      "#endif", 1)
    open(path, "w").write(s)
    print("  hid_service.cc: Haiku 분기 추가")
    done += 1

path = "%s/services/device/hid/BUILD.gn" % root
s = open(path).read()
if "hid_service_haiku" in s:
    print("  BUILD.gn: 이미 적용")
else:
    old = ('  if (is_fuchsia) {\n'
           '    sources += [\n'
           '      "hid_service_fuchsia.cc",\n'
           '      "hid_service_fuchsia.h",\n'
           '    ]\n'
           '  }')
    assert s.count(old) == 1, s.count(old)
    new = old + ('\n\n  if (is_haiku) {\n'
                 '    sources += [\n'
                 '      "hid_service_haiku.cc",\n'
                 '      "hid_service_haiku.h",\n'
                 '    ]\n'
                 '  }')
    open(path, "w").write(s.replace(old, new, 1))
    print("  BUILD.gn: Haiku 소스 추가")
    done += 1

print("hid haiku: %d" % done)
