# Export PKG_CONFIG_PATH from every script that runs ninja.
#
# gn regenerates build.ninja whenever a BUILD.gn changes, and //crypto asks
# pkg-config for nss. Haiku keeps its .pc files per architecture under
# /boot/system/develop/lib/x86/pkgconfig, which is not on the default search
# path, so without this the regen fails with "Package nss was not found" --
# and a retry loop scores that as a link failure that never reached ld.
import sys

OLD = "    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \\\n"
NEW = ("    PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig \\\n"
       "    PATH=/boot/home/config/non-packaged/bin-lowmem:$PATH \\\n")

for path in sys.argv[1:]:
    s = open(path).read()
    if "PKG_CONFIG_PATH" in s:
        print("%s: already set" % path)
        continue
    if s.count(OLD) != 1:
        print("%s: anchor found %d times, skipped" % (path, s.count(OLD)))
        continue
    open(path, "w").write(s.replace(OLD, NEW))
    print("%s: updated" % path)
