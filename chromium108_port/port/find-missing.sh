#!/bin/bash
# Every source a BUILD.gn names but the tarball does not ship.
#
# Finding these one ninja run at a time costs a full graph walk each. Ask gn
# for the whole picture instead: list every target's sources and check them.
cd /build/chromium108
export PATH=/build/xwrappers:$PATH
SYSROOT=/build/generated.x86only/cross-tools-x86/i586-pc-haiku
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig" PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
# ninja knows every input it needs; ask it, and see which do not exist.
ninja -C out/haiku-x86 -t deps 2>/dev/null | head -1 >/dev/null
ninja -C out/haiku-x86 -t inputs content_shell 2>/dev/null > /tmp/inputs.txt || true
if [ -s /tmp/inputs.txt ]; then
    echo "inputs listed: $(wc -l < /tmp/inputs.txt)"
    missing=0
    # Only inputs that are checked-in sources. Everything ninja will itself
    # produce lives under gen/ or obj/ or the out dir and is absent simply
    # because it has not been built -- counting those said 6491 "missing".
    while read -r f; do
        case "$f" in
            ../../*) ;;
            *) continue ;;
        esac
        rel="${f#../../}"
        [ -e "$rel" ] || { echo "  $rel"; missing=$((missing+1)); }
    done < /tmp/inputs.txt
    echo "missing: $missing"
else
    echo "ninja -t inputs did not work; falling back to the mojom lists"
    grep -rho "\"[a-zA-Z0-9_/.-]*\.mojom\"" content/test/BUILD.gn | tr -d '"' | sort -u |
      while read -r f; do [ -f "content/test/$f" ] || echo "content/test/$f"; done
fi
