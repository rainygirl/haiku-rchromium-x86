#!/usr/bin/env python3
"""Define the few targets gn insists exist, even though none are built.

`gn gen` reports "Unresolved dependencies" when something names a target that
its toolchain has no definition for. Seven such references survive here, all
from //chrome/test and //tools/perf -- neither of which this port builds --
into breakpad, angle's tests and dawn's common. Their BUILD.gn files wrap the
target in a list of operating systems, so on Haiku the target simply does not
exist and gn stops.

This adds Haiku to those guards. It does not make the code build: nothing
depends on these for content_shell, and if that ever changes they will fail
loudly at compile time rather than quietly produce something wrong. The 87
port dropped breakpad outright (0068-drop-crash-reporter-client-on-haiku);
that is the better end state, and reaching it means editing the users rather
than the providers, which is more churn than this stage needs.
"""
import sys

root = sys.argv[1]
edits = [
    # The root BUILD.gn's chromium_builder_perf group names angle_perftests
    # too. Narrowing gn_all left this one, which is why the same error kept
    # coming back after each apparently successful edit: three separate
    # references, found one at a time instead of by grepping for all of them
    # at the start.
    ("BUILD.gn",
     '      "//third_party/angle/src/tests:angle_perftests",\n',
     ""),
    ("third_party/angle/src/tests/restricted_traces/BUILD.gn",
     '    "$angle_root/src/tests:angle_perftests",\n',
     ""),
    # //chrome/test:performance_test_suite is the only thing that names
    # angle_perftests, and it is not built here. Making the target exist
    # instead means following angle's `test()` template into Chromium's own,
    # which has no Haiku arm -- deeper than this is worth for a test suite.
    # Excluding Haiku at the one consumer is smaller and clearer.
    ("chrome/test/BUILD.gn",
     "    if (!is_chromeos_lacros) {\n"
     '      data_deps += [ "//third_party/angle/src/tests:angle_perftests" ]',
     "    if (!is_chromeos_lacros && !is_haiku) {\n"
     '      data_deps += [ "//third_party/angle/src/tests:angle_perftests" ]'),
    ("third_party/breakpad/BUILD.gn",
     "if (is_linux || is_chromeos || is_android) {",
     "if (is_linux || is_chromeos || is_android || is_haiku) {"),
    # angle's test targets, named by //chrome/test:performance_test_suite.
    # The same guard appears twice, for end2end and for white-box.
    # The angle_tests group lists angle_perftests and the white-box perf
    # tests directly, so excluding them at chrome/test is not enough. These
    # are line removals rather than a rewritten block: the group continues
    # with several conditional deps += blocks, and an earlier attempt at
    # restructuring one of those in the root BUILD.gn cut it in the wrong
    # place and left every //chrome reference outside the guard.
    ("third_party/angle/src/tests/BUILD.gn",
     '    ":angle_end2end_tests",\n    ":angle_perftests",',
     '    ":angle_end2end_tests",'),
    ("third_party/angle/src/tests/BUILD.gn",
     '      ":angle_white_box_perftests",\n      ":angle_white_box_tests",',
     '      ":angle_white_box_tests",'),

    ("third_party/angle/src/tests/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_android || is_fuchsia || is_apple) {",
     "if (is_win || is_linux || is_chromeos || is_android || is_fuchsia ||\n"
     "    is_apple || is_haiku) {"),
    # ...and a second variant of the same list, without is_fuchsia, which
    # guards angle_perftests and angle_white_box_perftests.
    ("third_party/angle/src/tests/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_android || is_apple) {",
     "if (is_win || is_linux || is_chromeos || is_android || is_apple ||\n"
     "    is_haiku) {"),
    ("third_party/dawn/src/dawn/common/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_mac || is_fuchsia || is_android) {",
     "if (is_win || is_linux || is_chromeos || is_mac || is_fuchsia ||\n"
     "    is_android || is_haiku) {"),
]

done = 0
for rel, old, new in edits:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing: %s" % rel)
        continue
    # "already applied?" cannot be `new in s` when the edit removes a line:
    # the shortened text is a substring of the original, so the check passes
    # before anything is done and the edit is silently skipped. Ask whether
    # the thing being removed is still there instead.
    if old not in s:
        if new in s:
            continue
        print("  PATTERN NOT FOUND: %s" % rel)
        continue
    # Some guards repeat verbatim in one file; replace them all.
    open(path, "w").write(s.replace(old, new))
    print("  defined on Haiku: %s" % rel)
    done += 1
print("target guards: %d" % done)
