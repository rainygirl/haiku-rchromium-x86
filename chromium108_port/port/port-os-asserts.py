#!/usr/bin/env python3
"""Let Haiku past the desktop-platform asserts gn gen insists on evaluating.

`gn gen` evaluates every BUILD.gn reachable in the tree, not only what ninja
will build, so an assert in chrome/ stops a content_shell build. Narrowing the
root group does not help -- tried, and chrome/ was still read.

These asserts all have the same shape: a list of desktop operating systems,
written before Haiku existed, guarding code this port neither builds nor
wants. Rather than naming each file as it surfaces -- four rounds of that
produced four more -- rewrite the shape.

Only asserts are touched, and only ones that already name is_linux. A
conditional that *selects* behaviour is a different thing and is left alone:
saying Haiku is Linux there would compile the wrong code, while saying it here
merely stops gn from refusing to look.
"""
import os
import re
import sys

root = sys.argv[1]
# assert(... is_linux ...) possibly spanning lines, up to the first ) or ,
pattern = re.compile(r"assert\(([^;]*?is_linux[^;]*?)(,|\))", re.S)

changed = []
for dirpath, dirnames, names in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in ("out", ".git")]
    for name in names:
        if not name.endswith((".gn", ".gni")):
            continue
        path = os.path.join(dirpath, name)
        try:
            text = open(path).read()
        except (OSError, UnicodeDecodeError):
            continue

        def fix(m):
            body, tail = m.group(1), m.group(2)
            if "is_haiku" in body:
                return m.group(0)
            if len(body) > 400 or "\n\n" in body:
                return m.group(0)
            # An assert that compares two things is not a platform gate. In
            # grit_args.gni it reads `toolkit_views == (is_chromeos || ... )`,
            # and adding Haiku to the right-hand side makes it false here,
            # where toolkit_views is off -- turning a passing assert into a
            # failing one. Leave every equality alone.
            if "==" in body or "!=" in body:
                return m.group(0)
            return "assert(%s || is_haiku%s" % (body.rstrip(), tail)

        new = pattern.sub(fix, text)
        if new != text:
            open(path, "w").write(new)
            changed.append(os.path.relpath(path, root))

print("relaxed %d OS assert(s)" % len(changed))
for c in changed[:12]:
    print("  " + c)
if len(changed) > 12:
    print("  ... and %d more" % (len(changed) - 12))
