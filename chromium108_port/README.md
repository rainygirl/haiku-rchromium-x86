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
