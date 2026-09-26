#!/usr/bin/env python3
"""Which edits actually landed.

The port scripts cannot tell "already applied" from "the anchor is gone":
both end with `old` absent from the file, and both are skipped in silence.
That is fine while the tree only ever moves forward one milestone at a
time, and useless when the tree is replaced. So check the other end: for
every edit, is `new` in the file?

Usage: audit-anchors.py <chromium-root> <script.py> [<script.py> ...]
"""
import io
import os
import sys


def load_edits(path):
    """Run the script's prologue only, and take its edit table."""
    src = io.open(path, encoding="utf-8").read()
    for marker in ("for edit in edits:", "for rel, old, new in edits:"):
        if marker in src:
            src = src[:src.index(marker)]
            break
    ns = {"__name__": "__audit__", "sys": sys}
    sys.argv = ["audit", "/nonexistent"]
    try:
        exec(compile(src, path, "exec"), ns)
    except SystemExit:
        pass
    return ns.get("edits", []), ns.get("includes", [])


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    root = argv[1]
    total = landed = missing = absent = 0
    for script in argv[2:]:
        edits, _ = load_edits(script)
        name = os.path.basename(script)
        for edit in edits:
            rel, old, new = edit[0], edit[1], edit[2]
            total += 1
            path = os.path.join(root, rel)
            try:
                s = io.open(path, encoding="utf-8", errors="replace").read()
            except IOError:
                absent += 1
                print("파일 없음   %-28s %s" % (name, rel))
                continue
            if new in s:
                landed += 1
            elif old in s:
                print("미적용     %-28s %s" % (name, rel))
                missing += 1
            else:
                print("앵커 소실   %-28s %s" % (name, rel))
                print("            찾던 것: %s" % old.splitlines()[0][:90])
                missing += 1
    print()
    print("편집 %d건: 적용됨 %d, 손봐야 함 %d, 파일 없음 %d"
          % (total, landed, missing, absent))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
