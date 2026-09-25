#!/bin/bash
# Compile as far as it gets, collecting errors rather than stopping at the
# first. -k0 keeps going after failures, which is what makes this useful: the
# point at this stage is the shape of the work, not one error.
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null 2>&1
apt-get -qq install -y build-essential python3 pkg-config ninja-build libnss3-dev nodejs npm gperf >/dev/null 2>&1
# devtools-frontend runs its build steps through third_party/node/node.py,
# which looks for a bundled node under third_party/node/linux/node-linux-x64.
# The source tarball does not carry it -- that arrives with gclient sync --
# and the copy it would carry is an x86-64 binary, which is no use on this
# arm64 host anyway. Point the path at the distribution's node.
NODEDIR=/build/chromium108/third_party/node/linux/node-linux-x64/bin
mkdir -p "$NODEDIR"
ln -sf /usr/bin/node "$NODEDIR/node"

# esbuild is the other bundled binary, and it is an x86-64 Go executable from
# CIPD. On this arm64 host binfmt hands it to qemu, which then cannot find
# /lib64/ld-linux-x86-64.so.2 and says so once per devtools target -- 25 of
# them. Replace it with the arm64 build of the same tool; its command line is
# the part this build depends on and that has not changed.
ESBUILD=/build/chromium108/third_party/devtools-frontend/src/third_party/esbuild/esbuild
if [ -e "$ESBUILD" ]; then
	# The version has to match devtools' own, not merely be recent: esbuild's
	# JS client refuses a binary whose version differs -- "Host version
	# 0.14.13 does not match binary version 0.16.17". Read the pin out of
	# node_modules rather than guessing it.
	EV=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['version'])" \
	     /build/chromium108/third_party/devtools-frontend/src/node_modules/esbuild/package.json 2>/dev/null)
	[ -n "$EV" ] || EV=0.14.13
	HAVE=$("$ESBUILD" --version 2>/dev/null | tr -d ' \n')
	if [ "$HAVE" != "$EV" ]; then
		npm install --silent --prefix /tmp/esb "esbuild@$EV" >/dev/null 2>&1
		ARM64=/tmp/esb/node_modules/esbuild-linux-arm64/bin/esbuild
		[ -x "$ARM64" ] || ARM64=/tmp/esb/node_modules/@esbuild/linux-arm64/bin/esbuild
		if [ -x "$ARM64" ]; then
			cp "$ARM64" "$ESBUILD" && chmod 755 "$ESBUILD"
			echo "esbuild: $HAVE -> $EV (arm64)"
		else
			echo "esbuild: no arm64 build of $EV; devtools targets will fail"
		fi
	fi
fi
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
