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
# The bundled gn is an x86-64 ELF; this one was built here for arm64 so
# that the whole build -- gn, the cross-compiler, ninja -- runs native.
GN=tools/gn/out-arm64/gn
[ -x "$GN" ] || { echo "no gn at $GN"; exit 1; }
"$GN" gen out/haiku-x86 --args='
  target_os = "haiku"
  haiku_cross_bin = "/build/xwrappers"
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
  # The host here is arm64 Linux in a container. Naming the x64 toolchain
  # sends -m64 -msse3 to an aarch64 gcc, which answers "unrecognized
  # command-line option" -- 2,882 of them, the largest single group of
  # failures in the first real compile.
  host_toolchain = "//build/toolchain/linux:arm64"
  # V8 builds mksnapshot for the target word size and picks clang_x86 for a
  # 32-bit target. Pointing it at the gcc x86 toolchain traded one problem for
  # another: that is a *Linux* x86 toolchain, and this container has no 32-bit
  # glibc, so 592 compiles died on <bits/libc-header-start.h>. mksnapshot runs
  # on the host, so build it for the host word size; V8 can emit a 32-bit
  # snapshot from a 64-bit mksnapshot.
  v8_snapshot_toolchain = "//build/toolchain/linux:arm64"
  use_gold = false
  use_lld = false
  treat_warnings_as_errors = false
  v8_use_external_startup_data = false
' 2>&1 | head -30
