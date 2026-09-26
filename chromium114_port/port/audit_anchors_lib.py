"""The bit audit-anchors.py and suggest-anchors.py share: read a port
script's edit table without letting it edit anything."""
import io
import sys


def load_edits(path):
    src = io.open(path, encoding="utf-8").read()
    for marker in ("for edit in edits:", "for rel, old, new in edits:"):
        if marker in src:
            src = src[:src.index(marker)]
            break
    ns = {"__name__": "__audit__", "sys": sys}
    saved = sys.argv
    sys.argv = ["audit", "/nonexistent"]
    try:
        exec(compile(src, path, "exec"), ns)
    except SystemExit:
        pass
    finally:
        sys.argv = saved
    return ns.get("edits", [])
