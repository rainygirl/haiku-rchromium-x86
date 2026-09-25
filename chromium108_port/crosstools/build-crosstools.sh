#!/bin/bash
# Build a Haiku x86_gcc2 cross-compiler on Linux.
#
# The point of this is where Chromium gets compiled. The existing port builds
# natively on the VAIO because an earlier measurement found QEMU emulating x86
# Haiku 2.6x slower than the Atom itself. Cross-compiling is a different thing
# entirely: the compiler runs native on this host, and only the output is for
# Haiku. Ten cores against one 1.33 GHz Atom core changes days into hours.
#
# Target triple is i586-pc-haiku -- Haiku's configure picks that for x86_gcc2.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null
apt-get -qq install -y \
    git nasm bc autoconf automake texinfo flex bison gawk build-essential \
    unzip wget zip less zlib1g-dev libzstd-dev xorriso libtool \
    python3 >/dev/null
echo "host toolchain ready"

# Work inside the container's own filesystem, not the bind mount: the mount
# is macOS HFS+/APFS and case-insensitive, and Haiku's tree has pairs like
# Screenshot/ and screenshot/ that collide there. configure says so plainly --
# "You need a case-sensitive file-system to build Haiku."
cd /build
[ -d haiku ]      || git clone --depth 1 -q https://review.haiku-os.org/haiku
[ -d buildtools ] || git clone --depth 1 -q https://review.haiku-os.org/buildtools
echo "sources ready: $(cd haiku && git log --oneline -1)"

# The 1998 config.guess files in buildtools/legacy cannot name an arm64 host.
bash /work/refresh-config-guess.sh

# build_cross_tools forces CC="$CC -m32" before configuring gcc, with the
# comment "GCC 2 compiled for 64-bit on most systems is somewhat broken".
# That is about compiling gcc2 *for the host*, not about the target, and an
# arm64 host has no -m32 to give. The workaround was written for x86-64 hosts
# and arm64 almost certainly was never tried; drop it and find out. If gcc2
# really is broken when built 64-bit here, it will say so during its own
# bootstrap rather than silently.
sed -i 's|^export CC="\$CC -m32"$|# -m32 dropped: no such option on an arm64 host (see /work notes)|' \
    haiku/build/scripts/build_cross_tools
grep -n "m32 dropped" haiku/build/scripts/build_cross_tools

mkdir -p generated.x86gcc2
cd generated.x86gcc2
# x86_gcc2 alone is refused -- "Building a GCC2-only Haiku is no longer
# supported. Please configure the secondary architecture." The pair is what
# the VAIO actually runs: gcc2 for the primary ABI and a modern gcc for the
# secondary x86 one, which is why the 87 port compiles through -x86 suffixed
# wrappers. Both toolchains get built here.
../haiku/configure \
    --build-cross-tools x86_gcc2 \
    --build-cross-tools x86 \
    --cross-tools-source ../buildtools \
    -j"$(nproc)" 2>&1 | tail -25
echo "=== result ==="
ls cross-tools-x86_gcc2/bin 2>/dev/null | head -6
echo "--- secondary ---"
ls cross-tools-x86/bin 2>/dev/null | head -6
# Hand the finished toolchain back to the host through the bind mount.
if [ -d cross-tools-x86_gcc2 ]; then
    tar czf /work/cross-tools-x86_gcc2.tar.gz cross-tools-x86_gcc2 cross-tools-x86
    echo "packed $(du -sh /work/cross-tools-x86_gcc2.tar.gz | cut -f1) to /work"
fi
