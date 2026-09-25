#!/bin/bash
# Build only the secondary-ABI toolchain: modern gcc targeting Haiku x86.
#
# gcc2 cannot be built on an arm64 host -- gcc 2.95 has no configuration for
# aarch64, which is not a missing config.guess entry but a missing port, and
# it is 1999 code. But Chromium is not compiled with gcc2: the 87 port
# compiles through g++-x86, the *secondary* x86 ABI, which is a current gcc.
# If that half builds here, the cross-compile plan survives.
#
# --build-cross-tools x86 (not x86_gcc2) selects it.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null
apt-get -qq install -y \
    git nasm bc autoconf automake texinfo flex bison gawk build-essential \
    unzip wget zip less zlib1g-dev libzstd-dev xorriso libtool python3 >/dev/null

cd /build
[ -d haiku ]      || git clone --depth 1 -q https://review.haiku-os.org/haiku
[ -d buildtools ] || git clone --depth 1 -q https://review.haiku-os.org/buildtools

rm -rf generated.x86only
mkdir -p generated.x86only
cd generated.x86only
../haiku/configure \
    --build-cross-tools x86 \
    --cross-tools-source ../buildtools \
    -j"$(nproc)" 2>&1 | tail -20
echo "=== result ==="
ls cross-tools-x86/bin 2>/dev/null | head -14
if [ -d cross-tools-x86 ]; then
    tar czf /work/cross-tools-x86.tar.gz cross-tools-x86
    echo "packed $(du -sh /work/cross-tools-x86.tar.gz | cut -f1)"
fi
