#!/bin/bash
# The Haiku toolchain BUILD.gn names gcc-x86, g++-x86, ar-x86, nm-x86 and
# readelf-x86 -- the wrapper names the VAIO uses to select its secondary ABI.
# Here those are just the cross tools under another name.
set -e
CROSS=/build/generated.x86only/cross-tools-x86/bin
BIN=/build/xwrappers
mkdir -p "$BIN"
link() { ln -sf "$CROSS/i586-pc-haiku-$2" "$BIN/$1"; }
link gcc-x86     gcc
link g++-x86     g++
link ar-x86      ar
link nm-x86      nm
link readelf-x86 readelf
link ld-x86      ld
link strip-x86   strip
ls -l "$BIN" | awk '{print $9, $10, $11}' | tail -8
"$BIN/g++-x86" --version | head -1
