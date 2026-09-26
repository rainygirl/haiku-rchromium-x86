#!/bin/bash
# Does the cross-compiler actually produce Haiku x86 objects?
CC=/build/generated.x86only/cross-tools-x86/bin/i586-pc-haiku-g++
echo "=== version ==="
$CC --version | head -1
echo "=== does __HAIKU__ get defined? ==="
echo | $CC -dM -E - 2>/dev/null | grep -i "haiku\|__i386" | head -4
echo "=== compile something trivial ==="
cat > /tmp/a.cc <<'CC'
#include <cstdio>
int main() { printf("hello\n"); return 0; }
CC
if $CC -c /tmp/a.cc -o /tmp/a.o 2>/tmp/err.txt; then
    echo "  compiled ok"
    file /tmp/a.o 2>/dev/null || true
else
    echo "  FAILED:"; head -5 /tmp/err.txt
fi
echo "=== link it? (needs libroot) ==="
if $CC /tmp/a.cc -o /tmp/a 2>/tmp/err2.txt; then
    echo "  linked ok"; file /tmp/a
else
    echo "  link failed:"; head -4 /tmp/err2.txt
fi
