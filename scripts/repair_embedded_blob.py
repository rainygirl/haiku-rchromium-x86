#!/usr/bin/env python3
"""Repair the V8 embedded-builtins blob in a linked content_shell in place.

Legitimate because the blob carries no relocations: a correct link reproduces
embedded.o's .text byte-for-byte (the Sep 11 binary proves it, 0 bytes differ).
The VAIO's ld 2.17 under its low-memory flags overwrites the blob's interior
with stale bytes from earlier inputs, so copying the .text back is exactly what
the linker should have produced. Run verify_embedded_blob.py afterwards.

usage: repair_embedded_blob.py <content_shell> [<embedded.o>]
"""
import struct, sys, subprocess, shutil
binp = sys.argv[1]
eo = sys.argv[2] if len(sys.argv) > 2 else "/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native/obj/v8/v8_snapshot/embedded.o"

def sections(b):
    shoff = struct.unpack_from("<I", b, 0x20)[0]; shent = struct.unpack_from("<H", b, 0x2e)[0]
    shnum = struct.unpack_from("<H", b, 0x30)[0]; strndx = struct.unpack_from("<H", b, 0x32)[0]
    secs = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shent) for i in range(shnum)]
    stroff = secs[strndx][4]
    return [(b[stroff + s[0]:b.index(b"\0", stroff + s[0])].decode(), s) for s in secs]

ob = open(eo, "rb").read()
text = next(ob[s[4]:s[4] + s[5]] for n, s in sections(ob) if n == ".text")
# the .o must have no relocations against .text, or a byte copy is not a valid link result
rel = [n for n, s in sections(ob) if n in (".rel.text", ".rela.text")]
assert not rel, "embedded.o has relocations against .text (%s); byte copy would be wrong" % rel

b = bytearray(open(binp, "rb").read())
nm = subprocess.run(["nm", binp], capture_output=True, text=True).stdout
va = next(int(l.split()[0], 16) for l in nm.splitlines() if l.endswith(" v8_Default_embedded_blob_code_data_"))
e_phoff = struct.unpack_from("<I", b, 0x1c)[0]; phnum = struct.unpack_from("<H", b, 0x2c)[0]; phsz = struct.unpack_from("<H", b, 0x2a)[0]
off = None
for i in range(phnum):
    t, po, pv, pp, pf, pm = struct.unpack_from("<IIIIII", b, e_phoff + i * phsz)[:6]
    if t == 1 and pv <= va < pv + pf: off = po + (va - pv); break
assert off is not None
before = sum(1 for i in range(len(text)) if b[off + i] != text[i])
shutil.copy2(binp, binp + ".before-blob-repair")
b[off:off + len(text)] = text
open(binp, "wb").write(b)
print("repaired %d bytes at vaddr %#x (file %#x); %d bytes were wrong; backup at %s.before-blob-repair" % (len(text), va, off, before, binp))
