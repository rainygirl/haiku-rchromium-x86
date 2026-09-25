#!/bin/bash
# First gn gen for the Haiku target. The point is the error, not success.
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null 2>&1
apt-get -qq install -y python3 pkg-config libnss3-dev >/dev/null 2>&1
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
  haiku_sysroot = "/build/generated.x86only/cross-tools-x86/i586-pc-haiku"
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
  # The Haiku Ozone backend, carried over from the 87 port. Until now this
  # was ozone_platform = "headless", which builds but opens no window -- and
  # which also drags in EGL: headless_surface_factory.cc is a GLOzoneEGL
  # subclass, and with use_egl false on Haiku the EGL headers it needs are
  # never included. That accounted for 400 errors, and fixing them would
  # have meant repairing code this port does not use. The Haiku platform
  # draws through BView and has no EGL at all.
  ozone_platform = "haiku"
  ozone_platform_external = true
  ozone_extra_path = "//haiku_port/ozone_extra.gni"
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
  # SwiftShader is a software Vulkan/GL implementation, and pulling it in is
  # what asks EGL and Vulkan for a Haiku platform type they do not have. The
  # 87 port turned it off for the same reason; this machine has no GPU
  # acceleration worth the code either way.
  enable_swiftshader = false
  # use_egl = false is not a configuration 108 supports. ui/gl/gl_display.cc
  # is 1041 lines with no USE_EGL guard anywhere in it, ui/ozone/common
  # compiles four EGL files unconditionally, and every one of those is an
  # unguarded reference in code this port never runs -- 400 errors, and no
  # end in sight, because no in-tree platform builds that way.
  #
  # So EGL is on, and ANGLE is cut down instead. Its "#error Unsupported
  # OpenGL platform" lives inside ANGLE_ENABLE_OPENGL, and its Vulkan
  # backend is what drags in the Vulkan loader with its own "must be
  # modified for this OS". With both backends off, ANGLE builds with the
  # null renderer, which is a GL stack that does nothing -- exactly what a
  # port drawing through BView wants behind an API it never calls.
  use_egl = true
  angle_enable_gl = false
  angle_enable_vulkan = false
  # Debug info for a 200 MB binary on a machine that will never run a
  # debugger on it, at the cost of every compile and every link. The 87
  # port set all three to 0 as well.
  symbol_level = 0
  blink_symbol_level = 0
  v8_symbol_level = 0
  # From the 87 port args, which are the closest thing to a known-good
  # configuration this port has.
  proprietary_codecs = true
  ffmpeg_branding = "Chrome"
  # Torque is built for the host and its output is not host-independent:
  # the field types in torque-generated/ are chosen by V8_EXTERNAL_CODE_SPACE
  # as compiled into the torque binary. On an arm64 host that is on, so it
  # emitted CodeDataContainer fields; the x86 target has no pointer
  # compression, so CodeT is Code there, and 473 conversions failed -- all of
  # them "could not convert CodeDataContainer to CodeT {aka Code}", none of
  # them in code anybody wrote. Turning pointer compression off at the top
  # level reaches every toolchain, which makes external code space false in
  # both, which makes the two agree. Nothing is lost: pointer compression is
  # a 64-bit feature and this target is 32-bit.
  v8_enable_pointer_compression = false
  v8_enable_sandbox = false
  # use_nss_certs defaults to is_linux, and the host toolchain here IS Linux,
  # so the host build of //crypto pulled in NSS headers the container does
  # not have. Nothing on the Haiku side wants NSS: it is the client
  # certificate store Chromium uses on desktop Linux, and the host only
  # builds crypto to support host tools.
  # (No apostrophes in here -- the whole block is one single-quoted
  # shell argument, and one closed it.)
  use_nss_certs = false
  # DCHECKs are on by default in any non-official Chromium build, and they
  # are the reason 619 constexpr evaluations failed: V8 puts DCHECK_EQ inside
  # constexpr functions such as Context::SizeFor, and a DCHECK that is a real
  # check is not a constant expression for gcc. Beyond that, this build is
  # aimed at a 1.33 GHz Atom, where a release build with every DCHECK live is
  # not a trade worth making.
  dcheck_always_on = false
' 2>&1 | head -30
