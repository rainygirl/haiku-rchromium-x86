# Resolve a Haiku content_shell stack trace using the image map from the same
# run.
#
# Usage: python3 resolve2.py run3.log run3.images
#
# Chromium's in-signal-handler path prints " [0xADDR]" and nothing else. Both
# content_shell and every library are relocatable, and Haiku picks different
# addresses each run, so the only sound way to attribute a frame is to take
# the image bases the kernel actually used (listimage) from that same run.
#
# Attribution is still approximate inside content_shell: this build has
# symbol_level=0 and --gc-sections, so nm resolves an address to the nearest
# preceding surviving symbol. A frame reported as "f+0x2f7" where 0x2f7 is
# large is a frame in some other, discarded-symbol function -- read the offset
# before believing the name.
import bisect
import os
import re
import subprocess
import sys

log_path, img_path = sys.argv[1], sys.argv[2]

images = []  # (text_base, path)
for line in open(img_path):
    m = re.match(r"^\s*\d+\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\d+\s+\d+\s+(\S+)", line)
    if m:
        images.append((int(m.group(1), 16), m.group(3)))
    else:
        m = re.match(r"^\s*\d+\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\d+\s+\d+\s+(commpage)", line)
        if m:
            images.append((int(m.group(1), 16), "commpage"))
if not images:
    sys.exit("no images parsed from " + img_path)

# Bound each image by the size of its file where we can read it; the loader
# lays text and data out separately, so a plain "next base" bound would
# wrongly swallow a neighbouring image.
bounded = []
for base, path in images:
    try:
        size = os.path.getsize(path)
    except OSError:
        size = 0x400000  # commpage and friends: a generous default
    bounded.append((base, base + size, path))
bounded.sort()
starts = [b[0] for b in bounded]

sym_cache = {}


def symbols_for(path):
    """Sorted (value, name) for the code symbols of one image."""
    if path in sym_cache:
        return sym_cache[path]
    vals, names = [], []
    try:
        out = subprocess.run(["nm", "-n", path], capture_output=True,
                             text=True, timeout=600).stdout
    except Exception:
        out = ""
    for line in out.split("\n"):
        parts = line.split(None, 2)
        if len(parts) != 3:
            continue
        try:
            v = int(parts[0], 16)
        except ValueError:
            continue
        if parts[1] not in "tTwWvViI":
            continue
        vals.append(v)
        names.append(parts[2])
    sym_cache[path] = (vals, names)
    return sym_cache[path]


addrs = []
for line in open(log_path):
    m = re.match(r"^ \[0x([0-9a-f]+)\]\s*$", line)
    if m:
        addrs.append(int(m.group(1), 16))
if not addrs:
    sys.exit("no frames in " + log_path)

rows = []
for a in addrs:
    i = bisect.bisect_right(starts, a) - 1
    if i < 0 or a >= bounded[i][1]:
        rows.append((a, None, None, None))
        continue
    base, _end, path = bounded[i]
    off = a - base
    vals, names = symbols_for(path)
    if not vals:
        rows.append((a, path, off, None))
        continue
    j = bisect.bisect_right(vals, off) - 1
    if j < 0:
        rows.append((a, path, off, None))
    else:
        rows.append((a, path, off, (names[j], off - vals[j])))

mangled = [r[3][0] if r[3] else "" for r in rows]
plain = subprocess.run(["c++filt"], input="\n".join(mangled),
                       capture_output=True, text=True).stdout.split("\n")

for n, (a, path, off, sym) in enumerate(rows):
    short = os.path.basename(path) if path else "?"
    if sym is None:
        print("#%-3d 0x%08x  %-18s +0x%s" % (n, a, short,
                                             ("%x" % off) if off is not None else "?"))
    else:
        print("#%-3d 0x%08x  %-18s %s+0x%x" % (n, a, short,
                                               plain[n].strip(), sym[1]))
