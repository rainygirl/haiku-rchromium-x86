#!/bin/bash
# Compile as far as it gets, collecting errors rather than stopping at the
# first. -k0 keeps going after failures, which is what makes this useful: the
# point at this stage is the shape of the work, not one error.
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null 2>&1
apt-get -qq install -y build-essential python3 pkg-config ninja-build >/dev/null 2>&1
export PATH=/build/xwrappers:$PATH
SYSROOT=/build/generated.x86only/cross-tools-x86/i586-pc-haiku
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
cd /build/chromium108
ninja -C out/haiku-x86 -k0 -j"${JOBS:-10}" content_shell > /work/ninja.log 2>&1
echo "ninja exit=$?"
echo "=== progress ==="
grep -c "^\[" /work/ninja.log 2>/dev/null || true
tail -3 /work/ninja.log
echo "=== failed edges ==="
grep -c "^FAILED:" /work/ninja.log 2>/dev/null || echo 0
echo "=== why they failed ==="
grep -A200 "^FAILED:" /work/ninja.log 2>/dev/null |
  grep -oE "(not found|cannot execute|Segmentation fault|No such file or directory|error: [a-z][^\"]*)" |
  sed "s/[0-9]\+/N/g" | sort | uniq -c | sort -rn | head -12
echo "=== distinct error lines ==="
grep -oE "error: .*" /work/ninja.log 2>/dev/null | sed 's/[0-9]\+/N/g' | sort | uniq -c | sort -rn | head -15
echo "=== fatal errors (missing headers etc) ==="
grep -oE "fatal error: [^ ]+" /work/ninja.log 2>/dev/null | sort | uniq -c | sort -rn | head -12
