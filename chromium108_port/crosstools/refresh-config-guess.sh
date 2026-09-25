#!/bin/bash
# Teach the gcc2 buildtools what an arm64 host is.
#
# buildtools/legacy carries config.guess files from 1998. They predate
# aarch64, so on an Apple Silicon host they answer "unable to guess system
# type" and configure stops with "can not guess host type; you must specify
# one". Nothing about the *target* is wrong -- the cross-compiler still
# targets i586-pc-haiku -- it is only the host that cannot be named.
#
# The same tree already ships modern ones under buildtools/gcc, so the fix is
# a copy rather than a download: five config.guess and four config.sub.
set -e
cd /build
# gmp's copy is a wrapper that defers to configfsf.guess, which only exists
# beside it; pick a self-contained one instead.
NEWGUESS=$(for f in $(find buildtools/gcc -name config.guess); do
    grep -q configfsf "$f" || { echo "$f"; break; }
done)
NEWSUB=$(for f in $(find buildtools/gcc -name config.sub); do
    grep -q configfsf "$f" || { echo "$f"; break; }
done)
echo "using $NEWGUESS -> $(sh "$NEWGUESS")"
n=0
for f in $(find buildtools/legacy -name config.guess); do
    cp "$NEWGUESS" "$f"; n=$((n + 1))
done
for f in $(find buildtools/legacy -name config.sub); do
    cp "$NEWSUB" "$f"; n=$((n + 1))
done
echo "replaced $n files"
sh buildtools/legacy/binutils/config.guess
