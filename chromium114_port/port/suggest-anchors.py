#!/usr/bin/env python3
"""For every edit whose anchor is gone, show what the file says now.

audit-anchors.py says which 42 of 239 edits no longer match. Most of them
are the same shape -- a list of operating systems that gained or lost a
member between milestones -- so what is needed is not the whole file but
the lines around where the list used to be. This prints those.

Usage: suggest-anchors.py <chromium-root> <script.py> [<script.py> ...]
"""
import difflib
import io
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audit_anchors_lib import load_edits  # noqa: E402


def distinctive(old):
    """A line from `old` worth searching for: the longest one."""
    lines = [l.strip() for l in old.splitlines() if l.strip()]
    return max(lines, key=len) if lines else ""


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    root = argv[1]
    for script in argv[2:]:
        for edit in load_edits(script):
            rel, old, new = edit[0], edit[1], edit[2]
            path = os.path.join(root, rel)
            try:
                s = io.open(path, encoding="utf-8", errors="replace").read()
            except IOError:
                continue
            if new in s or old in s:
                continue
            print("=" * 72)
            print("%s  <-  %s" % (rel, os.path.basename(script)))
            print("--- 찾던 것 ---")
            for l in old.splitlines()[:4]:
                print("    " + l[:100])
            key = distinctive(old)
            lines = s.splitlines()
            # 가장 비슷한 줄 세 개와 그 주변
            scored = sorted(
                ((difflib.SequenceMatcher(None, key, l.strip()).ratio(), i)
                 for i, l in enumerate(lines) if l.strip()),
                reverse=True)[:3]
            print("--- 지금 그 자리 ---")
            for ratio, i in scored:
                if ratio < 0.5:
                    continue
                lo, hi = max(0, i - 2), min(len(lines), i + 4)
                print("  %s:%d  (닮은 정도 %.2f)" % (rel, i + 1, ratio))
                for j in range(lo, hi):
                    print("    %s%s" % (">" if j == i else " ", lines[j][:100]))
                print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
