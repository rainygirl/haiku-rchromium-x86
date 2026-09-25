#!/bin/bash
# Give the cross-compiler Haiku's headers and x86 libraries.
#
# They come from the VAIO itself rather than from a Haiku source build: the
# target is that exact installation, and its headers and libs are what the
# finished binary will run against. 78 MB of headers and 22 MB of x86
# secondary-ABI libraries, 18 MB packed.
set -e
ROOT=/build/generated.x86only/cross-tools-x86/i586-pc-haiku
mkdir -p "$ROOT"
cd "$ROOT"
tar xzf /work/haiku-sysroot.tar.gz
# `include`, not `sys-include`. sys-include is only consulted while gcc
# itself is being built; a finished cross-gcc searches
# <prefix>/i586-pc-haiku/include, which is what -E -v prints.
mkdir -p include
for d in headers/posix headers/glibc; do
    [ -d "$d" ] && cp -a "$d"/. include/ 2>/dev/null || true
done
# headers/config keeps its directory: BeBuild.h asks for <config/HaikuConfig.h>
# by that path, so flattening it breaks the very first include.
cp -a headers/config include/ 2>/dev/null || true
# The os headers are nested one directory per kit, and Haiku puts every kit
# directly on the include path rather than requiring <os/kernel/OS.h>.
cp -a headers/os/. include/ 2>/dev/null || true
for d in headers/os/*/; do
    [ -d "$d" ] && cp -a "$d". include/ 2>/dev/null || true
done
# And the libraries next to the compiler's own, flat. The tarball carries
# them under lib/x86/ because that is where the secondary ABI lives on Haiku,
# but the cross-ld only has SEARCH_DIR for <prefix>/i586-pc-haiku/lib, so a
# nested x86/ leaves libroot.so invisible to it.
if [ -d lib/x86 ]; then
    cp -a lib/x86/. lib/ 2>/dev/null || true
    rm -rf lib/x86
fi
echo "include: $(find include -maxdepth 1 -name '*.h' | wc -l) headers at top level"
echo "lib: $(ls lib/*.so 2>/dev/null | wc -l) shared libraries"
ls include/stdio.h include/OS.h 2>/dev/null
