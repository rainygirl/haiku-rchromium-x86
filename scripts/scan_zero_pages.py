#!/usr/bin/env python3
"""Reject a linked binary that contains whole pages of zeros where code lives.

Why this exists, separately from verify_embedded_blob.py: that script compares
V8's 1 MB blob against embedded.o, and the 2026-09-20 link was broken *outside*
the blob as well. Four page-aligned runs of zeros -- 7, 59, 67 and 9 pages --
had been written into .text and .rodata, one of them (59 pages at file 0x1d75000)
straight over Builtins_JSEntry, which is why x.com died reading zeros out of
V8's builtin code and looked for a day like a V8 code-range mapping bug.

Whole zero pages, page-aligned, in a file ld reported no error writing, are a
lost writeback, not a linker layout decision. A 4096-byte run of zeros cannot
be real x86 code: alignment padding is nops or 0xcc, and no function is 4 kB of
`add %al,(%eax)`. .rodata legitimately holds a few isolated zero pages (the
known-good Sep 15 binary has four), so only runs are rejected there.

usage: scan_zero_pages.py <binary>        exit 0 = clean
"""
import struct
import sys

RODATA_RUN_LIMIT = 4  # consecutive zero pages in .rodata that count as damage
PAGE = 4096


def sections(b):
    shoff = struct.unpack_from("<I", b, 0x20)[0]
    shent = struct.unpack_from("<H", b, 0x2E)[0]
    shnum = struct.unpack_from("<H", b, 0x30)[0]
    strndx = struct.unpack_from("<H", b, 0x32)[0]
    secs = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shent)
            for i in range(shnum)]
    stroff = secs[strndx][4]
    out = []
    for s in secs:  # Elf32_Shdr: name type flags addr offset size ...
        n = b[stroff + s[0]:b.index(b"\0", stroff + s[0])].decode()
        out.append((n, s[4], s[5]))
    return out


def main():
    path = sys.argv[1]
    b = open(path, "rb").read()
    secs = {n: (off, size) for n, off, size in sections(b)}
    ranges = []
    for name in (".text", ".rodata"):
        if name in secs:
            off, size = secs[name]
            ranges.append((name, off, off + size))
    if not ranges:
        sys.exit("no .text/.rodata in %s" % path)

    bad = []
    for name, start, end in ranges:
        first = (start + PAGE - 1) // PAGE * PAGE
        runs = []
        page = first
        while page + PAGE <= end:
            if b[page:page + PAGE] == b"\0" * PAGE:
                if runs and runs[-1][0] + runs[-1][1] * PAGE == page:
                    runs[-1][1] += 1
                else:
                    runs.append([page, 1])
            page += PAGE
        limit = 1 if name == ".text" else RODATA_RUN_LIMIT
        for off, n in runs:
            if n >= limit:
                bad.append((name, off, n))

    if not bad:
        print("ZERO-PAGE SCAN OK: %s" % path)
        return 0
    print("ZERO-PAGE SCAN FAILED -- DO NOT INSTALL: %s" % path)
    for name, off, n in bad:
        print("  %-8s file 0x%08x  %d zero page%s"
              % (name, off, n, "" if n == 1 else "s"))
    return 1


if __name__ == "__main__":
    sys.exit(main())
