#!/bin/bash
# Build gn for arm64 so the whole build can run native.
#
# The bundled buildtools/linux64/gn is an x86-64 ELF, which is why the
# container was started with --platform linux/amd64. But the Haiku
# cross-compiler was built in an arm64 container and is an arm64 binary, so
# under amd64 emulation /bin/sh reports it "not found" -- 3,377 compiles
# failed that way, and the message says "not found" rather than anything
# about architecture, which is what made it look like a PATH problem twice.
#
# Building gn natively removes the conflict, and native is 7.5x faster than
# the emulation besides.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null
apt-get -qq install -y build-essential python3 ninja-build >/dev/null
cd /build/chromium108/tools/gn
rm -rf out-arm64
# gen.py writes clang++ into the ninja file and there is no clang here.
CC=gcc CXX=g++ AR=ar python3 build/gen.py --out-path=out-arm64 \
    --no-last-commit-position --no-strip 2>&1 | tail -2
sed -i "s/^cxx = clang++/cxx = g++/; s/^ld = clang++/ld = g++/; s/ -Werror / /g" \
    out-arm64/build.ninja
cat > out-arm64/last_commit_position.h <<'HDR'
#ifndef OUT_LAST_COMMIT_POSITION_H_
#define OUT_LAST_COMMIT_POSITION_H_
#define LAST_COMMIT_POSITION_NUM 0
#define LAST_COMMIT_POSITION "108.0.5359.124 (tarball)"
#endif
HDR
ninja -C out-arm64 gn 2>&1 | tail -3
ls -la out-arm64/gn
./out-arm64/gn --version
