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

A .rodata run is a weaker signal than a .text one: a genuinely zero const
object trips it, and one does -- libaom's 128 kB `wedge_mask_buf`, 32 zero
pages, in the arm64 sibling's binary. Look the address up with `nm` before
believing a .rodata-only failure.

usage: scan_zero_pages.py <binary>        exit 0 = clean
"""
import struct
import sys

RODATA_RUN_LIMIT = 4  # consecutive zero pages in .rodata that count as damage
PAGE = 4096


def sections(b):
    """(name, file offset, size) per section, for ELF32 or ELF64."""
    if b[:4] != b"\x7fELF":
        sys.exit("not an ELF file")
    if b[4] == 2:  # ELFCLASS64
        shoff = struct.unpack_from("<Q", b, 0x28)[0]
        shent, shnum, strndx = struct.unpack_from("<HHH", b, 0x3A)
        fmt, off_i, size_i = "<IIQQQQIIQQ", 4, 5
    else:
        shoff = struct.unpack_from("<I", b, 0x20)[0]
        shent, shnum, strndx = struct.unpack_from("<HHH", b, 0x2E)
        fmt, off_i, size_i = "<IIIIIIIIII", 4, 5
    secs = [struct.unpack_from(fmt, b, shoff + i * shent) for i in range(shnum)]
    stroff = secs[strndx][off_i]
    out = []
    for s in secs:  # Shdr: name type flags addr offset size link info align ..
        n = b[stroff + s[0]:b.index(b"\0", stroff + s[0])].decode()
        out.append((n, s[off_i], s[size_i], s[off_i + 4]))
    return out


def main():
    path = sys.argv[1]
    b = open(path, "rb").read()
    secs = {n: (off, size, align) for n, off, size, align in sections(b)}
    ranges = []
    for name in (".text", ".rodata"):
        if name in secs:
            off, size, align = secs[name]
            ranges.append((name, off, off + size, align))
    if not ranges:
        sys.exit("no .text/.rodata in %s" % path)

    bad = []
    for name, start, end, align in ranges:
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
            # A run that ends on one of the section's own alignment boundaries
            # is padding the linker inserted to place what follows. lld gives
            # arm64 .text a 64 kB alignment and pads four zero pages in front
            # of V8's embedded blob; that is layout, not damage.
            if align >= PAGE and (off + n * PAGE) % align == 0:
                continue
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
