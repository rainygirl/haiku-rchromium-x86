#!/usr/bin/env python3
"""Add Haiku to third-party OS conditionals that content_shell reaches.

Chromium's own code mostly asks `is_posix`, which Haiku is. Vendored
third-party BUILD.gn files tend to enumerate operating systems instead, and a
missing arm of that chain is not a warning -- gn stops with "Undefined
identifier in string expansion" or picks a Windows path.

Each entry here is one such chain, added as it was hit. Haiku goes wherever
Linux goes: the generated headers, the sources and the defines a POSIX system
wants are the same ones.

Some entries are not third-party and are not built either -- gn gen evaluates
the whole tree, so an assert in chrome/ stops the build for a target that
only wants content_shell. Narrowing the root group does not help; that was
tried.
"""
import os
import sys

root = sys.argv[1]
edits = [
    # libxml picks a directory of configure-generated headers per OS.
    ("third_party/libxml/BUILD.gn",
     "if (is_linux || is_chromeos || is_android || is_nacl || is_fuchsia) {\n"
     '  os_include = "linux"',
     "if (is_linux || is_chromeos || is_android || is_nacl || is_fuchsia ||\n"
     "    is_haiku) {\n"
     '  os_include = "linux"'),

    # gn gen evaluates every BUILD.gn in the tree, chrome/ included, so its
    # asserts have to pass even though nothing there is built. This one gates
    # //chrome/browser/resources/intro.
    ("components/signin/features.gni",
     "enable_dice_support = is_linux || is_mac || is_win || is_fuchsia",
     "enable_dice_support = is_linux || is_mac || is_win || is_fuchsia ||\n"
     "                      is_haiku"),
]

# Some OS lists repeat verbatim across files. Rewriting them by pattern beats
# naming each file, and reports how many matched so a silent zero is visible.
bulk = [
    # //ui/views asserts toolkit_views, and this port sets it false. Nothing
    # under views is built here, but gn still evaluates the file because
    # //chrome/test names views_perftests. The 87 port never met this: its
    # qtwebengine source has no chrome/ at all.
    ("assert(toolkit_views)",
     "assert(toolkit_views || is_haiku)  # evaluated, never built, on Haiku"),
    ("assert(is_linux || is_chromeos || is_win || is_mac || is_fuchsia)",
     "assert(is_linux || is_chromeos || is_win || is_mac || is_fuchsia ||\n"
     "       is_haiku)"),
]

bulk_total = 0
for old, new in bulk:
    hits = 0
    for dirpath, _, names in os.walk(root):
        if "/out/" in dirpath:
            continue
        for name in names:
            if not name.endswith((".gn", ".gni")):
                continue
            fp = os.path.join(dirpath, name)
            try:
                text = open(fp).read()
            except (OSError, UnicodeDecodeError):
                continue
            if old not in text:
                continue
            open(fp, "w").write(text.replace(old, new))
            hits += 1
    if hits:
        print("  %d file(s): %s" % (hits, old.split("(")[0] + "(...)"))
    bulk_total += hits
print("bulk OS-list rewrites: %d file(s)" % bulk_total)

done = 0
skipped = 0
for rel, old, new in edits:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing, skipped: %s" % rel)
        continue
    if new in s:
        skipped += 1
        continue
    if old not in s:
        print("  PATTERN NOT FOUND in %s" % rel)
        continue
    open(path, "w").write(s.replace(old, new, 1))
    print("  patched %s" % rel)
    done += 1
print("third-party OS chains: %d patched, %d already done" % (done, skipped))
