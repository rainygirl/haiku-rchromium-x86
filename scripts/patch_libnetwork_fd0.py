#!/usr/bin/env python3
"""Take the close(0) out of a built libnetwork.so.

The proper fix is haiku_kernel_patches/K0002, which needs Haiku rebuilt. This
does the same thing to the shared library that is already installed, so a
browser can run on a machine as it stands.

res_ndestroy() closes ext->kq and ext->resfd unless they are -1, and on Haiku
they are never anything but 0, because __res_vinit() memsets the struct and
nothing here ever opens a kqueue. Both branches are replaced with nops, which
is what current Haiku master does with #ifndef __HAIKU__.

It patches a copy, never the system file: point content_shell at it with

    LIBRARY_PATH=<outdir>:%A/lib:/boot/system/lib

Usage:  patch_libnetwork_fd0.py <libnetwork.so> <output>

The byte patterns are verified before writing and the offsets are found by
searching, not hardcoded, so this survives a rebuild of the library as long
as the compiler emits the same shape.
"""
import sys

# mov 0x370(%edi),%eax ; cmp $-1,%eax ; jne <close>   -- ext->kq
# mov 0x374(%edi),%eax ; cmp $-1,%eax ; jne <close>   -- ext->resfd
PATTERNS = [
    ("ext->kq", bytes.fromhex("8b8770030000") + bytes.fromhex("83f8ff")),
    ("ext->resfd", bytes.fromhex("8b8774030000") + bytes.fromhex("83f8ff")),
]


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    data = bytearray(open(argv[1], "rb").read())
    patched = 0
    for name, pat in PATTERNS:
        at = data.find(pat)
        if at < 0:
            print("%s: 패턴을 찾지 못함 -- 이미 고쳐졌거나 다른 빌드" % name)
            continue
        jne = at + len(pat)
        if data[jne] != 0x75:
            print("%s: 0x%x 에 jne 가 없음 (0x%02x). 건드리지 않음"
                  % (name, jne, data[jne]))
            continue
        data[jne:jne + 2] = b"\x90\x90"
        print("%s: 0x%x 의 jne 를 nop 으로" % (name, jne))
        patched += 1
    if patched == 0:
        print("바꾼 것이 없다")
        return 1
    open(argv[2], "wb").write(bytes(data))
    print("%s 에 씀 (%d 곳)" % (argv[2], patched))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
