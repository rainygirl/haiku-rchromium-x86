#!/usr/bin/env python3
"""Keep gn gen from evaluating all of chrome/ for a content_shell build.

`gn gen` evaluates every target reachable from the root whether ninja will
build it or not, so asserts in code this port does not want still have to
pass -- `assert(toolkit_views)` in //ui/views, reached through
//chrome/test:browser_tests, and a fixed OS list in several
//chrome/browser/resources targets.

There is no dotfile setting for this in gn 2065: `root` there names the source
directory, not a target, and `root_target` is rejected outright with
"Assignment had no effect". So the root group is replaced on Haiku, which is
what the 87 port did to the qtwebengine tree's BUILD.gn.

The whole group is swapped rather than wrapping its body in an else. The body
is a deps list followed by some twenty conditional blocks that append to it,
several of which name //chrome; an else around only the list leaves those
appending to a Haiku build. Tried that first -- it still pulled in
chrome/test.
"""
import re
import sys

path = sys.argv[1] + "/BUILD.gn"
s = open(path).read()

if "R Chromium builds content_shell" in s:
    print("BUILD.gn: already done")
    sys.exit(0)

start = s.index('group("gn_all") {')
# Find the group's real end by balancing braces. Looking for the first "\n}\n"
# stops inside the body -- gn_all contains conditional blocks whose closing
# brace sits at column 2, but it also contains deps lists that end with a line
# of "}" at column 0 inside a nested scope, and cutting there left every
# //chrome reference after it outside the else.
depth = 0
i = start
while True:
    c = s[i]
    if c == "{":
        depth += 1
    elif c == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
    i += 1
while end < len(s) and s[end] == "\n":
    end += 1

replacement = '''# R Chromium builds content_shell and nothing else. On Haiku the usual
# gn_all is not merely unnecessary but unevaluatable: it reaches chrome/,
# whose test and resource targets assert on toolkit_views and on a fixed list
# of operating systems that Haiku is not in.
if (is_haiku) {
  group("gn_all") {
    testonly = true
    deps = [ "//content/shell:content_shell" ]
  }
} else {
''' + s[start:end].rstrip("\n") + "\n}\n"

s = s[:start] + replacement + s[end:]
open(path, "w").write(s)
print("BUILD.gn: gn_all replaced on Haiku")
