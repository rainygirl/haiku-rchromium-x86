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
    # The sampling profiler's signal-based stack copier reads linux/futex.h
    # and suspends a thread with a signal. Neither exists here; Haiku joins
    # nacl and apple in not building it.
    ("base/BUILD.gn",
     "    if (!is_nacl && !is_apple) {\n"
     "      sources += [\n"
     '        "profiler/stack_base_address_posix.cc",',
     "    if (!is_nacl && !is_apple && !is_haiku) {\n"
     "      sources += [\n"
     '        "profiler/stack_base_address_posix.cc",'),

    # Two more webrtc targets want <ifaddrs.h>, which Haiku keeps in
    # headers/bsd along with getifaddrs in libbsd.
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("rtc_base") {\n',
     'rtc_library("rtc_base") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("threading") {\n',
     'rtc_library("threading") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),

    # The crash reporter once more -- crashpad this time, reached through
    # //components/crash/core/app. crashpad's address_types.h ends in
    # "#error Unhandled OS type", and teaching it about Haiku would only
    # move the problem: its client wants a handler process, ptrace and
    # /proc. The 87 port dropped it (patches 0060, 0068, U0007) and this is
    # the same cut. Fuchsia already opts out of exactly this, so the guards
    # gain a second name.
    ("content/shell/BUILD.gn",
     "  if (is_fuchsia) {\n"
     '    deps += [ "//third_party/fuchsia-sdk/sdk/fidl/fuchsia.ui.policy" ]\n'
     "  } else {\n"
     "    deps += [\n"
     '      "//components/crash/content/browser",\n'
     '      "//components/crash/core/app",\n'
     "    ]\n"
     "  }",
     "  if (is_fuchsia) {\n"
     '    deps += [ "//third_party/fuchsia-sdk/sdk/fidl/fuchsia.ui.policy" ]\n'
     "  } else if (!is_haiku) {\n"
     "    deps += [\n"
     '      "//components/crash/content/browser",\n'
     '      "//components/crash/core/app",\n'
     "    ]\n"
     "  }"),

    # The source that subclasses CrashReporterClient goes with them.
    # Compiling it without the library that defines the base class leaves
    # its whole vtable undefined at the final link, which the 87 port found
    # out the slow way. The removal has to sit in content_shell_app, which
    # owns those sources -- gn rejects "sources -=" for an entry the target
    # does not have.
    ("content/shell/BUILD.gn",
     "  if (!is_fuchsia) {\n"
     "    deps += [\n"
     '      "//components/crash/core/app",\n'
     '      "//components/crash/core/app:test_support",\n'
     "    ]\n"
     "  }",
     "  if (!is_fuchsia && !is_haiku) {\n"
     "    deps += [\n"
     '      "//components/crash/core/app",\n'
     '      "//components/crash/core/app:test_support",\n'
     "    ]\n"
     "  }\n"
     "  if (is_haiku) {\n"
     "    sources -= [\n"
     '      "app/shell_crash_reporter_client.cc",\n'
     '      "app/shell_crash_reporter_client.h",\n'
     "    ]\n"
     "  }"),
    # Chromium already knows gcc cannot take ##__VA_ARGS__ in
    # standards-conforming mode -- the comment in this very block says so,
    # and the answer is -std=gnu++17 rather than -std=c++17. Haiku was not
    # in the list of systems that reach it, so it fell through to the
    # hardcoded -std=c++17 further down and V8's DEFINE_PARAMETERS() macros
    # produced 702 "expected identifier before ',' token".
    ("build/config/compiler/BUILD.gn",
     '  if (is_linux || is_chromeos || is_android || (is_nacl && is_clang) ||\n'
     '      current_os == "aix") {',
     '  if (is_linux || is_chromeos || is_android || (is_nacl && is_clang) ||\n'
     '      is_haiku || current_os == "aix") {'),
    # webrtc's net_helpers wants <ifaddrs.h>, which Haiku has in headers/bsd
    # along with the getifaddrs implementation in libbsd. Same treatment as
    # libevent: the directory goes on this target's include path and no
    # further.
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("net_helpers") {\n  sources = [',
     'rtc_library("net_helpers") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'
     '  sources = ['),
    # libevent is the one target that needs Haiku's BSD headers, for
    # sys/queue.h. It gets them through a config rather than through the
    # toolchain's global flags, because headers/bsd also carries a
    # sys/param.h defining ALIGN(p) -- and dav1d defines ALIGN(ll, a).
    ("third_party/libevent/BUILD.gn",
     '    include_dirs = [ "haiku" ]',
     '    include_dirs = [ "haiku" ]\n'
     '    configs += [ "//build/config/haiku:bsd" ]'),
    # The crash reporter is the last thing dragging breakpad's Linux client
    # into the build, through crash_key_lib. Chromium already has the switch
    # for a platform without one -- use_crash_key_stubs, which Fuchsia sets --
    # and it compiles crash_key_stubs.cc instead. That is the whole fix; no
    # dependency needs cutting by hand. breakpad's client is what wanted
    # sys/ucontext.h, linux/limits.h and link.h, none of which Haiku has, and
    # it could not have worked anyway: it reads /proc.
    ("components/crash/core/common/BUILD.gn",
     "  use_crash_key_stubs = is_fuchsia",
     "  use_crash_key_stubs = is_fuchsia || is_haiku"),
    # libxslt picks its config.h directory by OS and has no fallback, so on
    # Haiku the include_dirs line named nothing and 19 translation units
    # could not find config.h. libxml, one directory over, already lists
    # Haiku alongside Linux for exactly this; libxslt just never got the
    # same treatment. Linux's config.h is the right one -- the HAVE_* set it
    # records is POSIX, and the places Haiku differs are not in it.
    ("third_party/libxslt/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "linux/config.h" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    sources += [ "linux/config.h" ]'),
    ("third_party/libxslt/BUILD.gn",
     "  if (is_linux || is_chromeos || is_android || is_fuchsia) {\n"
     '    include_dirs = [ "linux" ]',
     "  if (is_linux || is_chromeos || is_android || is_fuchsia || is_haiku) {\n"
     '    include_dirs = [ "linux" ]'),
    # content_shell's data_deps pull in breakpad's dump_syms and
    # minidump_stackwalk on every posix, which includes Haiku now. They are
    # symbol tools, not part of running a browser, and breakpad's client
    # wants link.h, sys/ucontext.h and sys/syscall.h -- none of which Haiku
    # has. The 87 port dropped the crash reporter outright
    # (0068-drop-crash-reporter-client-on-haiku); this is the same call.
    ("content/shell/BUILD.gn",
     "  if (is_posix) {\n"
     "    data_deps += [\n"
     '      "//third_party/breakpad:dump_syms",\n'
     '      "//third_party/breakpad:minidump_stackwalk",\n'
     "    ]\n"
     "  }",
     "  if (is_posix && !is_haiku) {\n"
     "    data_deps += [\n"
     '      "//third_party/breakpad:dump_syms",\n'
     '      "//third_party/breakpad:minidump_stackwalk",\n'
     "    ]\n"
     "  }"),
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
    # Whether an edit has already been applied cannot be asked the same way
    # for every edit, and getting it wrong is silent both ways.
    #
    #   removal (the replacement is a fragment of what it replaces): `new` is
    #   a substring of the original too, so `new in s` is true before anything
    #   is done and the edit is skipped forever.
    #
    #   addition (the replacement contains what it replaces, as perfetto's
    #   does): `old` is still there afterwards, so `old in s` is true again
    #   and the edit is applied a second and a third time.
    #
    # Which of the two an edit is can be read off the edit itself.
    if new in old:
        # A removal: the replacement is a fragment of what it replaces, so it
        # is a substring of the original too and "new in s" is true before
        # anything is done. Ask whether the thing being removed is still there.
        already = old not in s
    else:
        # An addition or a substitution. "new in s" is not enough either:
        # liftoff-assembler-ia32.h already contained three "uintptr_t
        # offset_imm" of its own, so the check passed and the fourteen
        # "uint32_t offset_imm" were never touched. Take the replacement out
        # of the text first, then ask whether any unpatched occurrence
        # remains.
        already = old not in s.replace(new, "")
    if already:
        continue
    if old not in s:
        print("  PATTERN NOT FOUND: %s" % rel)
        continue
    # Some guards repeat verbatim in one file; replace them all.
    open(path, "w").write(s.replace(old, new))
    print("  defined on Haiku: %s" % rel)
    done += 1
print("target guards: %d" % done)
