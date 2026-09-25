#!/bin/bash
# First gn gen for the Haiku target. The point is the error, not success.
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null 2>&1
apt-get -qq install -y python3 pkg-config >/dev/null 2>&1
# pkg-config has to find Haiku's .pc files, not the container's. They came
# over with the sysroot; the 87 port sets the same variable on the VAIO.
SYSROOT=/build/generated.x86only/cross-tools-x86/i586-pc-haiku
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"

cd /build/chromium108
GN=buildtools/linux64/gn
[ -x "$GN" ] || { echo "no gn at $GN"; exit 1; }
"$GN" gen out/haiku-x86 --args='
  target_os = "haiku"
  target_cpu = "x86"
  is_debug = false
  is_component_build = false
  use_ozone = true
  use_sysroot = false
  enable_rust = false
  is_clang = false
  use_custom_libcxx = false
  use_aura = true
  enable_plugins = false
  enable_pdf = false
  enable_nacl = false
  enable_print_preview = false
  enable_remoting = false
  enable_widevine = false
  use_dbus = false
  use_udev = false
  use_cups = false
  use_glib = false
  use_gio = false
  use_kerberos = false
  use_pulseaudio = false
  use_alsa = false
  use_libpci = false
  use_gtk = false
  use_x11 = false
  ozone_auto_platforms = false
  ozone_platform = "headless"
  ozone_platform_headless = true
  use_v8_context_snapshot = false
  # The source tarball has no bundled clang -- that arrives with gclient sync,
  # and update.py refuses with "Did you run gclient sync?". The host toolchain
  # uses the system compiler instead, which is what distribution packagers do.
  # This is separate from the Haiku toolchain, which is gcc either way.
  clang_use_chrome_plugins = false
  # ...and name the gcc host toolchain explicitly. Leaving it to default picks
  # //build/toolchain/linux:clang_x64, which asks update.py for the bundled
  # clang before anything else happens.
  host_toolchain = "//build/toolchain/linux:x64"
  # V8 builds mksnapshot for the target word size, and picks clang_x86 for a
  # 32-bit target unless told otherwise.
  v8_snapshot_toolchain = "//build/toolchain/linux:x86"
  use_gold = false
  use_lld = false
  treat_warnings_as_errors = false
  v8_use_external_startup_data = false
' 2>&1 | head -30
