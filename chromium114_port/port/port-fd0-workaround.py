#!/usr/bin/env python3
"""The close(0) workaround, and the probe that found what needs it.

Haiku's libnetwork closes a descriptor it does not own. __res_vinit() zeroes
the resolver's state and returns early on some failures, leaving
res_state_ext::kq and ::resfd at 0 rather than -1; res_ndestroy() then closes
both, and 0 is stdin. Chromium notices at the next legitimate close of that
descriptor: it gets EBADF and ScopedFD's PCHECK takes the process down.

haiku_kernel_patches/K0002-libnetwork-res_ndestroy-closes-fd-0.patch fixes it
where it belongs. This is what keeps a browser running on a system that does
not have that fix yet -- which today is every system.

Two things live here, and only the first one changes behaviour:

  - close() of fd 0, 1 or 2, and any failing close, stops being fatal.
  - RCH_FD0_PROBE=1 prints where each one came from, as an offset into the
    application image, because Haiku relocates every image differently and a
    bare address cannot be attributed to anything without that. This is the
    instrumentation the bug was found with. It is off by default: on a stock
    system it fires several times per page load and says nothing a user can
    act on.

Written as a whole-block replace rather than an edit chain because the block
was applied by hand during the hunt before it was ever a script, so the tree
this runs against may already have a version of it.
"""
import re
import sys

root = sys.argv[1]
path = "%s/base/files/scoped_file.cc" % root
s = open(path).read()
before = s

PROBE = '''#if defined(__HAIKU__)
namespace {
// RCH_FD0_PROBE=1: say whether stdin, stdout and stderr are open as each image
// loads. The first close of stdin happens inside libnetwork's res_ndestroy(),
// which runs from a library initialiser, so the question is which image was
// being loaded at the time.
struct HaikuStdioProbe {
  HaikuStdioProbe() {
    if (getenv("RCH_FD0_PROBE") == nullptr)
      return;
    fprintf(stderr,
            "[RCH] image load: fd0=%s fd1=%s fd2=%s\\n",
            fcntl(0, F_GETFD) != -1 ? "open" : "closed",
            fcntl(1, F_GETFD) != -1 ? "open" : "closed",
            fcntl(2, F_GETFD) != -1 ? "open" : "closed");
  }
};
HaikuStdioProbe g_haiku_stdio_probe;
}  // namespace
#endif
'''

CLOSE = '''#if BUILDFLAG(IS_HAIKU)
  // Not fatal here. See the file comment: libnetwork closes stdin, and the
  // process that notices is not the one that did it. fd 0, 1 and 2 are treated
  // the same whether the close succeeded or not -- the first close of stdin is
  // the bug and every close after it is fallout.
  if (ret != 0 || fd <= 2) {
    if (getenv("RCH_FD0_PROBE") != nullptr) {
      static addr_t text_base = 0;
      if (text_base == 0) {
        image_info info;
        int32 cookie = 0;
        while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
          if (info.type == B_APP_IMAGE) {
            text_base = (addr_t)info.text;
            break;
          }
        }
      }
      addr_t ra = (addr_t)__builtin_return_address(0);
      PLOG(ERROR) << "HAIKU: close(" << fd << ") "
                  << (ret == 0 ? "ok" : "FAILED") << " from 0x" << std::hex
                  << (unsigned long)(ra - text_base) << std::dec
                  << " (image-relative)";
    }
    ret = 0;
  }
#endif
'''

HEADERS = '''#if defined(__HAIKU__)
#include <OS.h>
#include <fcntl.h>
#include <image.h>
#include <stdio.h>
#include <stdlib.h>

#include <ostream>
#endif
'''

# The headers the two blocks need. stdlib.h is the one a hand-applied version
# will be missing, because getenv() is new here.
# A lambda, not the string: re.sub() processes escapes in a replacement, and
# these blocks contain a C++ "\\n" that would become a real newline in the
# middle of a string literal. That is exactly what happened the first time.
s = re.sub(r"#if defined\(__HAIKU__\)\n#include <OS\.h>\n.*?\n#endif\n",
           lambda m: HEADERS, s, count=1, flags=re.S)
if HEADERS not in s:
    s = s.replace('#include "build/build_config.h"\n',
                  '#include "build/build_config.h"\n\n' + HEADERS, 1)

# The stdio probe: replace an existing one, or add it after the headers.
if "HaikuStdioProbe" in s:
    s = re.sub(r"#if defined\(__HAIKU__\)\nnamespace \{\n.*?HaikuStdioProbe g_haiku_stdio_probe;\n\}  // namespace\n#endif\n",
               lambda m: PROBE, s, count=1, flags=re.S)
else:
    s = s.replace(HEADERS, HEADERS + "\n" + PROBE, 1)

# The close() workaround: replace an existing one, or add it before the PCHECK
# that it exists to keep out of the way of.
if "BUILDFLAG(IS_HAIKU)" in s and "if (ret != 0 || fd <= 2)" in s:
    s = re.sub(r"#if BUILDFLAG\(IS_HAIKU\)\n  // .*?\n#endif\n",
               lambda m: CLOSE, s, count=1, flags=re.S)
else:
    anchor = "  PCHECK(0 == ret);"
    assert s.count(anchor) == 1, s.count(anchor)
    s = s.replace(anchor, CLOSE + "\n" + anchor, 1)

if s == before:
    print("fd0 workaround: 변경 없음")
else:
    open(path, "w").write(s)
    print("fd0 workaround: scoped_file.cc 갱신")

# The other half of the probe: which shared-memory handle ended up holding a
# descriptor that should not have been free. port-haiku-platform.py puts
# HaikuWarnZeroFd() in base/memory/platform_shared_memory_handle.cc and logs
# unconditionally; the same reasoning applies, so it gets the same switch.
path = "%s/base/memory/platform_shared_memory_handle.cc" % root
try:
    s = open(path).read()
except FileNotFoundError:
    s = ""
if s and "getenv(\"RCH_FD0_PROBE\")" not in s and "HaikuWarnZeroFd" in s:
    old = "  if (fd != 0 && readonly_fd != 0)\n    return;"
    assert s.count(old) == 1, s.count(old)
    s = s.replace(old,
                  "  if (fd != 0 && readonly_fd != 0)\n"
                  "    return;\n"
                  "  if (getenv(\"RCH_FD0_PROBE\") == nullptr)\n"
                  "    return;", 1)
    if "#include <stdlib.h>" not in s:
        s = s.replace("#if defined(__HAIKU__)\n#include <OS.h>",
                      "#if defined(__HAIKU__)\n#include <stdlib.h>\n\n#include <OS.h>", 1)
    open(path, "w").write(s)
    print("fd0 workaround: platform_shared_memory_handle.cc 조용해짐")
elif s:
    print("fd0 workaround: platform_shared_memory_handle.cc 변경 없음")
