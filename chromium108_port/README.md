# Chromium 108 port (x86, Qt-free)

Work in progress. The goal is a 32-bit Haiku build of vanilla Chromium 108
with no Qt anywhere in its provenance.

## Why 108, and why at all

The shipping port is Chromium 87, whose source came from QtWebEngine's
snapshot because HaikuPorts had already carried that exact snapshot. On
2026-09-25 x.com moved its logged-out login flow to a build whose entry module
uses **top-level await**, which ES modules got in Chrome 89. V8 8.7 stops at
`SyntaxError: Unexpected reserved word`, the page stays blank, and no URL,
user agent or retry gets around it -- measured, not assumed. Signing in is
therefore impossible on 87, and it is only the first modern-web feature to
close that door.

108 is the newest milestone that still meets every constraint this machine
imposes:

| constraint | why 108 works |
| --- | --- |
| Atom Z520 is 32-bit only | V8 keeps its ia32 backend; Chrome still ships Win32 |
| built with gcc, not clang | `is_clang` is still a GN arg with a non-clang path |
| no Rust toolchain for Haiku x86 | `enable_rust = false` is the default until 114 |
| x.com's syntax and APIs | 108 covers everything measured, up to Chrome 103 |

From 119 `enable_rust = build_with_chromium`, and the arm64 port records that
by trunk skia's `bridge_rust_side` makes Rust impossible to switch off. That
is the ceiling.

## What this has to supply

Chromium has no Haiku support of any kind -- the arm64 port counted 13 files
out of 505,289 mentioning Haiku, all inside vendored Rust crates. So:

1. platform detection (`OS_HAIKU`, `BUILDFLAG(IS_HAIKU)`, the POSIX set)
2. a GN target OS and toolchain for the x86 secondary ABI
3. the POSIX gaps: no `/proc`, no fork+exec sandbox, different thread APIs
4. the Ozone/BeAPI backend, which the 87 port already has and which ports
   forward rather than being rewritten

Two reference implementations bracket this work: `../chromium87_overlay`
(same architecture, same compiler, 21 milestones older) and the arm64 port at
Chromium 154 (vanilla source, systematic injection, 46 milestones newer).
Neither applies directly. The 87 patches are addressed at a
qtwebengine-chromium layout with `chromium/`, `gn/` and `ninja/` at the
checkout root, so they do not even path-match a vanilla tarball.

## gn gen passes (2026-09-25)

`build.ninja` is generated: 4.3 MB, 24,864 build edges. Chromium 108 accepts
Haiku as a target OS and resolves the whole graph for it.

The eleven unresolved dependencies it started with were all test and
crash-reporting targets -- angle's tests, breakpad, dawn's common -- reached
from `//chrome/test` and `//tools/perf`, neither of which is built here.
`port-target-guards.py` adds Haiku to the OS lists that would otherwise leave
those targets undefined, and removes the three references to
`angle_perftests`, whose definition goes through Chromium's `test()` template
and has no Haiku arm.

Two mistakes in that last part are worth keeping, because both cost a round
each:

- **Three references, found one at a time.** chrome/test, the `angle_tests`
  group, and `chromium_builder_perf` in the root BUILD.gn all name
  angle_perftests. Each fix looked like it had worked and the same error came
  back. Grepping for every reference at the start would have found all three
  in one go.
- **An "already applied?" check that was always true.** For an edit that
  *removes* a line, `if new in s: continue` passes before anything is done --
  the shortened text is a substring of the original. It reported "0 patched"
  with no warning. The check now asks whether the text being removed is still
  present.

And one thing that was never a dependency problem at all: gn warned that
`build_angle_perftests` "was set as a build argument but never appeared in a
declare_args() block", which meant angle's BUILD.gn was not being read. The
argument was a guess at a fix and removing it is what let gn gen finish.

### What had to be got past, in order

**gn gen evaluates the whole tree.** Not the requested target's graph -- every
BUILD.gn it can reach. So asserts in chrome/ stop a content_shell build.
Narrowing the root `gn_all` group does not help; that was tried twice, once
per-arm and once by replacing the group, and chrome/ was read regardless.
`port-os-asserts.py` relaxes the desktop-platform asserts instead: 37 files,
by pattern rather than by name, after four rounds of naming files produced
four more.

**Equality asserts must be left alone.** `grit_args.gni` reads
`assert(toolkit_views == (is_chromeos || is_fuchsia || is_linux || ...))`.
Adding `|| is_haiku` to the right-hand side turns a passing assert into a
failing one, because toolkit_views is off here. The rule now skips any assert
containing `==` or `!=`.

**Do not force toolkit_views = false as a global arg.** Its default is derived
from the OS list, which already excludes Haiku. Setting it globally also sets
it for the *host* toolchain, where is_linux is true -- and then the equality
above fails for the host.

**The tarball has no bundled clang.** `update.py` answers "Did you run gclient
sync?". Both `host_toolchain` and `v8_snapshot_toolchain` have to name the gcc
toolchains explicitly; left to default they pick `clang_x64` and `clang_x86`
and ask for a clang that was never downloaded.

**pkg-config needs the Haiku sysroot.** `PKG_CONFIG_PATH`,
`PKG_CONFIG_LIBDIR` and `PKG_CONFIG_SYSROOT_DIR` all point into
`cross-tools-x86/i586-pc-haiku`; the VAIO's 136 `.pc` files came over with it.
Without this, nss stops the configure with "Could not run pkg-config".

## Compiling (2026-09-25)

30,491 edges, 257 failures left. The count fell in large steps, and every one
of the big ones was a single root:

| errors | cause |
| --- | --- |
| 3,377 | the cross-compiler is an arm64 binary; the container was amd64 |
| 2,882 | `host_toolchain` named x64 on an arm64 host: `-m64 -msse3` to an aarch64 gcc |
| 8,287 | V8 and perfetto keep their own OS chains, and both ended at Haiku |
| 596 | `v8_snapshot_toolchain` named Linux x86; no 32-bit glibc in the container |
| 578 | `V8_HAS_MALLOC_USABLE_SIZE`: Haiku's libroot has no such function |
| 539 | `export-template.h`'s own static_asserts, which gcc 13 fails |

### "not found" was never about PATH

`/bin/sh: /build/xwrappers/g++-x86: not found` was read as a PATH problem
twice, and the toolchain was changed to use absolute paths because of it. The
path was right both times. Linux says "not found" when it cannot run a
binary's *interpreter or architecture*, and the cross-compiler had been built
in an arm64 container while the build ran under `--platform linux/amd64` for
the sake of the bundled x86-64 `gn`. Building gn for arm64 removed the
conflict -- and native is 7.5x faster than the emulation anyway.

### Adding an OS to a POSIX set opens holes

`V8_OS_HAIKU` put Haiku inside every `V8_OS_POSIX` branch, which is right, and
then wrong wherever V8 means "posix but not X". `malloc_usable_size` is the
first: Haiku belongs beside AIX on the exclusion side. Expect more of these
rather than fewer as Haiku reaches further into the tree.

### Not every error is about Haiku

`export-template.h` fails on gcc 13 for reasons that have nothing to do with
the target -- 539 of them, from four static_asserts that test the header's own
macro machinery. The macros work; the self-test does not expand as they
expect. 108 is built with clang upstream, so this is the first of a class:
things that break because the compiler is gcc, not because the OS is Haiku.
