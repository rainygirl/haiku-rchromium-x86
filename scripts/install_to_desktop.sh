#!/bin/sh
# Install R Chromium into ~/RChromium and put a launcher on the Desktop.
#
# Run on the build machine. Idempotent.
#
# Why a launcher script rather than the binary itself on the Desktop: the
# 188 MB executable needs content_shell.pak, icudtl.dat and the locales
# directory beside it -- PathProviderHaiku derives DIR_ASSETS from the
# executable's own path (patch 0078) -- so the binary cannot be moved away
# from them, and the Desktop is no place for a 200 MB application directory.
# The launcher is also where the flags this port still needs are recorded, so
# double-clicking gives the same browser the run script does.
#
# Tracker draws a file's own BEOS:ICON in preference to the icon of its MIME
# type, so a shell script shows the Chromium icon exactly as an application
# would. That is the whole trick here; there is no .rdef to compile because
# scripts carry no resources, only attributes.
set -eu

BUILD=${1:-/boot/home/rchromium-chromium87-fast/chromium/out/rchromium_native}
APPDIR=${2:-/boot/home/RChromium}
LAUNCHER="/boot/home/Desktop/R Chromium"
# The blue Chromium vector icon (HVIF, "ncif" magic, #8FC4E7 over #539CCF).
# It is kept as a plain file, extracted with `catattr -r BEOS:ICON` from the
# earlier Desktop entry, rather than compiled from an .rdef: re-authoring HVIF
# without Icon-O-Matic is not something to do for an icon that already exists,
# and a file survives the entry it came from being deleted.
ICON_FILE=${ICON_FILE:-/boot/home/rchromium-icon.hvif}
SIG="application/x-vnd.rainygirl-RChromium"

if [ ! -x "$BUILD/content_shell" ]; then
    echo "not built: $BUILD/content_shell" >&2
    exit 66
fi

# Nothing in the packaging path checks the architecture of what it copies --
# AddFilesToPackage does not read ELF headers, and an arm64 binary was once
# packaged into an x86 image because of it. Check here instead.
arch=$(file -b "$BUILD/content_shell")
case "$arch" in
    *"Intel 80386"*) ;;
    *) echo "wrong architecture: $arch" >&2; exit 65 ;;
esac

mkdir -p "$APPDIR"
cp -f "$BUILD/content_shell" "$APPDIR/content_shell"
for f in content_shell.pak shell_resources.pak icudtl.dat snapshot_blob.bin \
         v8_context_snapshot.bin; do
    [ -f "$BUILD/$f" ] && cp -f "$BUILD/$f" "$APPDIR/$f"
done
[ -d "$BUILD/locales" ] && cp -rf "$BUILD/locales" "$APPDIR/"

cat > "$LAUNCHER" <<'LAUNCH'
#!/bin/sh
# R Chromium. Chromium 87 content_shell on Haiku's own Ozone backend --
# no Qt: `readelf -d` on the binary lists libbe, not libQt5*.
APPDIR=/boot/home/RChromium

# Blink aborts in font_cache.cc without a fontconfig configuration to load,
# and Haiku installs no /etc/fonts.
FONTCONFIG_FILE=${FONTCONFIG_FILE:-/boot/home/rchromium-fonts.conf}
export FONTCONFIG_FILE

# --single-process: the renderer still dies on startup in multi-process mode.
# --in-process-gpu: viz hands the software output device a null widget across
# the process boundary, so the pixels never reach the BView.
exec "$APPDIR/content_shell" \
    --ozone-platform=haiku \
    --single-process \
    --disable-gpu \
    --in-process-gpu \
    "$@"
LAUNCH
chmod +x "$LAUNCHER"

# Attributes, not resources. Deskbar and Tracker read attributes, and a file
# whose icon is only a resource shows the generic one -- several hours were
# lost to that on this machine before.
# -c VICN, not -t VICN: addattr takes a four-character type ID only through
# -c ("attribute type VICN is not valid" otherwise), and -f feeds it the raw
# HVIF bytes so nothing has to hex-encode a binary blob through a shell
# argument.
if [ -f "$ICON_FILE" ]; then
    addattr -f "$ICON_FILE" -c VICN BEOS:ICON "$LAUNCHER"
fi
addattr -t mime BEOS:TYPE text/x-shellscript "$LAUNCHER"
addattr -t mime BEOS:APP_SIG "$SIG" "$LAUNCHER"

echo "installed:"
ls -l "$APPDIR"
echo
listattr "$LAUNCHER"
