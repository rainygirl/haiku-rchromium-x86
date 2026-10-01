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
# Skia's raster pipeline is SIMD only when compiled by clang, so SkOpts.cpp
# goes to clang (port/clang-x86) and everything else to g++. A dispatching
# script rather than a GN change: the command lines ninja hashes stay the
# same, so nothing else rebuilds.
command -v clang++ >/dev/null || apt-get -qq install -y clang >/dev/null
install -m 755 "$(dirname "$0")/clang-x86" "$BIN/clang-x86"
rm -f "$BIN/g++-x86"
cat > "$BIN/g++-x86" <<WRAP
#!/bin/bash
case " \$* " in
	*" -o obj/skia/skia_core_and_effects/SkOpts.o "*) exec "$BIN/clang-x86" "\$@" ;;
esac
exec "$CROSS/i586-pc-haiku-g++" "\$@"
WRAP
chmod 755 "$BIN/g++-x86"
link ar-x86      ar
link nm-x86      nm
link readelf-x86 readelf
link ld-x86      ld
link strip-x86   strip
ls -l "$BIN" | awk '{print $9, $10, $11}' | tail -8
"$BIN/g++-x86" --version | head -1
