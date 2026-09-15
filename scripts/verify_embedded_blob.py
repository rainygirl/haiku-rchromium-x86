#!/usr/bin/env python3
"""Verify that a linked content_shell carries V8's embedded-builtins blob intact.

Why this exists: on the VAIO's old binutils, the low-memory link mode
(--no-keep-memory, no --build-id) silently wrote ~88 KB of another input
section over the middle of embedded.o's 1 MB .text. The blob's first bytes
were fine, so nothing noticed, and V8 then jumped into Blink code the moment
any JavaScript ran (SIGILL at Builtins_JSEntryTrampoline+0x10). Days were lost
chasing that as a 32-bit V8 defect. embedded builtins are fully
position-independent, so a correct link reproduces embedded.o's .text
byte-for-byte -- the Sep 11 binary does. Anything else is a broken link.

usage: verify_embedded_blob.py <content_shell> [<embedded.o>]   exit 0 = intact
"""
import struct, sys, subprocess
binp = sys.argv[1]
eo = sys.argv[2] if len(sys.argv) > 2 else "/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native/obj/v8/v8_snapshot/embedded.o"

def elf_sections(b):
    shoff = struct.unpack_from("<I", b, 0x20)[0]; shentsize = struct.unpack_from("<H", b, 0x2e)[0]
    shnum = struct.unpack_from("<H", b, 0x30)[0]; shstrndx = struct.unpack_from("<H", b, 0x32)[0]
    secs = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shentsize) for i in range(shnum)]
    stroff = secs[shstrndx][4]
    def name(n): e = b.index(b"\0", stroff + n); return b[stroff + n:e].decode()
    return [(name(s[0]), s) for s in secs]

def text_of_object(path):
    b = open(path, "rb").read()
    for name, s in elf_sections(b):
        if name == ".text": return b[s[4]:s[4] + s[5]]
    raise SystemExit("no .text in " + path)

def symbol_vaddr(path, sym):
    out = subprocess.run(["nm", path], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        if ln.endswith(" " + sym): return int(ln.split()[0], 16)
    raise SystemExit("symbol %s not found in %s" % (sym, path))

def file_bytes_at_vaddr(b, va, n):
    e_phoff = struct.unpack_from("<I", b, 0x1c)[0]; phnum = struct.unpack_from("<H", b, 0x2c)[0]; phsz = struct.unpack_from("<H", b, 0x2a)[0]
    for i in range(phnum):
        t, po, pv, pp, pf, pm = struct.unpack_from("<IIIIII", b, e_phoff + i * phsz)[:6]
        if t == 1 and pv <= va < pv + pf: o = po + (va - pv); return b[o:o + n]
    raise SystemExit("vaddr %#x not in any PT_LOAD" % va)

ref = text_of_object(eo)
b = open(binp, "rb").read()
va = symbol_vaddr(binp, "v8_Default_embedded_blob_code_data_")
got = file_bytes_at_vaddr(b, va, len(ref))
bad = [i for i in range(len(ref)) if got[i] != ref[i]]
if not bad:
    print("EMBEDDED BLOB OK: %d bytes at %#x match embedded.o" % (len(ref), va)); sys.exit(0)
print("EMBEDDED BLOB CORRUPT: %d/%d bytes differ, first at +%#x, last at +%#x -- DO NOT INSTALL" % (len(bad), len(ref), bad[0], bad[-1]))
sys.exit(1)
