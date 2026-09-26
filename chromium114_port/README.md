# Chromium 114 port (x86, Qt-free)

A continuation of `chromium108_port/`, not a restart. The scripts and the
Haiku source files carry over; what does not carry over is where every patch
anchors, because 114 moved the code around.

## Why 114 and not 110

108 runs on this machine -- it builds, it links, Haiku's loader accepts it, a
window opens, text renders, TLS works, and **top-level await runs**, which is
what the whole port was for. What it does not do is render x.com, and the
reason is that x.com now asks for JavaScript newer than 108:

	TypeError: n.filter(...).map(...).toSorted is not a function
	  at abs.twimg.com/x-web/x-web/assets/locale-SttZuWL1.js

`Array.prototype.toSorted` is ES2023 and shipped in Chrome 110. V8 10.8
already has it, behind `--harmony-change-array-by-copy`, and turning that on
made `[].toSorted` appear -- and the page still did not mount, so there is
more behind it.

110 would buy exactly that one feature. The cost of moving is the same
whatever the target, because the work is re-anchoring ~242 patches, so the
target should be the highest version the constraints allow:

| constraint | where it bites |
| --- | --- |
| Atom Z520 is 32-bit only | not a problem: V8 keeps its ia32 backend |
| built with gcc, not clang | gets worse with every milestone |
| no Rust toolchain for Haiku x86 | `enable_rust = false` is the default **through 114**; from 119 skia's `bridge_rust_side` makes Rust impossible to switch off |

So 114 is the last milestone where Rust is off by default, and that is the
line this port stops at while it is still a gcc build.

## What carries over

| | |
| --- | --- |
| `port/port-target-guards.py` | 66 gn edits |
| `port/port-haiku-platform.py` | 162 source edits |
| `port/port-compile-fixes.py` | 8 edits |
| `port/port-os-detection.py` | 6 edits |
| `files/` | 39 whole files this port adds |

Every edit carries its own "already applied" test and prints
`PATTERN NOT FOUND` when its anchor is gone, so the first run against 114 is
itself the worklist.

## What is already known to need looking at

- the gn args: some were renamed or removed between 108 and 114
- `v8_snapshot_toolchain` still needs a 32-bit mksnapshot; the i686 Linux
  cross toolchain and qemu arrangement should carry over unchanged
- the Ozone/BeAPI backend, which moved once already from 87 to 108
- `K0002` (Haiku's libnetwork closing fd 0) is a system bug, not a Chromium
  one, and applies here too
