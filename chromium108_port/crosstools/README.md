# Cross-compiling Haiku x86 from a Linux container

Verified 2026-09-25: a binary compiled on an arm64 Mac ran on the VAIO and
called `get_system_info()` successfully.

    cross-compiled on arm64, running on Haiku
    cpus=2  max_pages=521550

This matters because of where Chromium gets built. AGENTS.md concluded "build
on the VAIO", and that conclusion stands for what it measured -- QEMU
emulating a whole x86 Haiku on Apple Silicon, at 2.6x the Atom's time per
object. Cross-compiling is a different arrangement: the compiler runs native
on the host and only its output is for Haiku. Ten cores at 56 ms per object
against one contended Atom core at 98 ms.

## The route

    colima start --cpu 10 --memory 24 --disk 120 --vm-type vz --arch aarch64
    docker run -v ~/haiku-x86-build:/work -v haikubuild:/build debian:bookworm \
        bash /work/build-x86-only.sh      # the toolchain, ~97 MB packed
    # then, with a sysroot tarred off the VAIO with `tar -czh`:
    bash /work/install-sysroot.sh
    bash /work/testcross.sh               # compiles and links a test program

## Five things that have to be got right

**Build inside the container's own filesystem, not the bind mount.** Haiku's
tree has `Screenshot/` and `screenshot/`, and configure says so: "You need a
case-sensitive file-system to build Haiku." A macOS bind mount is not one.

**x86_gcc2 cannot be built alone.** configure refuses: "Building a GCC2-only
Haiku is no longer supported. Please configure the secondary architecture."

**...and on arm64 it cannot be built at all.** gcc 2.95 is 1999 code with no
aarch64 host configuration -- `Configuration aarch64-unknown-linux-gnu not
supported`. Replacing the 1998 `config.guess` files gets past the first error
and straight into that one. **It does not matter**: Chromium compiles through
the *secondary* x86 ABI, which is a current gcc. `--build-cross-tools x86`
alone gives `i586-pc-haiku-g++ (GCC) 13.3.0`, which is exactly what the 87
port's `g++-x86` wrapper runs.

**Take the sysroot off the VAIO with `tar -czh`.** Haiku's `libroot.so` and
`libstdc++.so` under `develop/lib/x86` are symlinks into the live system
(`../../../lib/x86/...`). Without `-h` they arrive dangling -- 497 broken
links -- and ld reports `cannot find -lroot` while `ls` shows the file there.
18 MB of links becomes 181 MB of real files.

**Headers go in `include/`, not `sys-include/`.** sys-include is only read
while gcc itself is being built; a finished cross-gcc searches
`<prefix>/i586-pc-haiku/include`, as `g++ -E -v` will tell you. Keep
`headers/config/` nested: `BeBuild.h` includes `<config/HaikuConfig.h>` on the
first line of anything.

## Not viable: an emulated x86-64 container

Measured rather than assumed, because it would have been the obvious way to
get a host gcc2:

| | per object | stable |
| --- | --- | --- |
| native arm64 | 56 ms | yes |
| emulated x86-64 | 395-520 ms | no -- cc1plus took SIGSEGV |
| VAIO Atom (recorded) | 98 ms | yes |

Slower than the Atom it was meant to replace, and `cc1plus` segfaulted under
concurrency. One run of 48 compiles passed cleanly, which is why this needs
saying: a single clean run is not evidence of stability, and the crash came
back on the next measurement.
