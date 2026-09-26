#!/usr/bin/env python3
"""Define the few targets gn insists exist, even though none are built.

`gn gen` reports "Unresolved dependencies" when something names a target that
its toolchain has no definition for. Seven such references survive here, all
from //chrome/test and //tools/perf -- neither of which this port builds --
into breakpad, angle's tests and dawn's common. Their BUILD.gn files wrap the
target in a list of operating systems, so on Haiku the target simply does not
exist and gn stops.

This adds Haiku to those guards. It does not make the code build: nothing
depends on these for content_shell, and if that ever changes they will fail
loudly at compile time rather than quietly produce something wrong. The 87
port dropped breakpad outright (0068-drop-crash-reporter-client-on-haiku);
that is the better end state, and reaching it means editing the users rather
than the providers, which is more churn than this stage needs.
"""
import sys

root = sys.argv[1]
edits = [
    # gpu_info_collector_fuchsia.cc calls angle::GetSystemInfo, so the dep
    # that provides it has to come with it.
    ("gpu/config/BUILD.gn",
     "  if (is_linux || is_chromeos || is_mac || is_fuchsia) {\n"
     '    deps += [ "//third_party/angle:angle_gpu_info_util" ]',
     "  if (is_linux || is_chromeos || is_mac || is_fuchsia || is_haiku) {\n"
     '    deps += [ "//third_party/angle:angle_gpu_info_util" ]'),

    # The Ozone shared-image path. These are dmabuf-backed images, which
    # Haiku has none of -- the sources were excluded earlier for that
    # reason, but the factory is still constructed by name. Excluding it
    # here too, where the block is gated on use_ozone rather than on having
    # dmabuf.
    ("gpu/command_buffer/service/BUILD.gn",
     "    if (use_ozone) {\n"
     "      sources += [\n"
     '        "shared_image/gl_ozone_image_representation.cc",',
     "    if (use_ozone && !is_haiku) {\n"
     "      sources += [\n"
     '        "shared_image/gl_ozone_image_representation.cc",'),

    # The image transport surface. Haiku draws through BView and never asks
    # for a native GL surface; the Linux file wants a GLX or EGL one.
    ("gpu/ipc/service/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "image_transport_surface_linux.cc" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    sources += [ "image_transport_surface_linux.cc" ]'),
    # The fontconfig include path for ui/gfx. font_fallback_linux.cc and the
    # two files beside it are on Haiku's list now, and all three open with
    # <fontconfig/fontconfig.h>.
    ("ui/gfx/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    deps += [ "//third_party/fontconfig" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    deps += [ "//third_party/fontconfig" ]'),
    # ...and the file that defines gfx::FallbackFontData. It goes through
    # fontconfig, which this port builds.
    ("ui/gfx/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    sources += [\n"
     '      "font_fallback_linux.cc",\n'
     '      "font_fallback_linux.h",',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     "    sources += [\n"
     '      "font_fallback_linux.cc",\n'
     '      "font_fallback_linux.h",'),
    # blink's font cache and its theme. font_cache_linux.cc goes through the
    # font service for fallback, which reaches the same fontconfig this port
    # now builds; layout_theme_linux.cc is colours and metrics with nothing
    # Linux about it.
    ("third_party/blink/renderer/platform/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    sources += [\n"
     '      "fonts/linux/font_cache_linux.cc",',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     "    sources += [\n"
     '      "fonts/linux/font_cache_linux.cc",'),
    ("third_party/blink/renderer/core/layout/build.gni",
     "if (is_linux || is_chromeos) {\n"
     "  blink_core_sources_layout += [\n"
     '    "layout_theme_linux.cc",',
     "if (is_linux || is_chromeos || is_haiku) {\n"
     "  blink_core_sources_layout += [\n"
     '    "layout_theme_linux.cc",'),

    # Memory instrumentation reads /proc/<pid>/smaps on Linux. Haiku has no
    # /proc. The Fuchsia file looked like the do-nothing implementation and
    # is not -- it includes <lib/zx/job.h> -- which is the second time a
    # *_fuchsia.cc has fooled me this session, so Haiku gets its own.
    ("services/resource_coordinator/public/cpp/memory_instrumentation/BUILD.gn",
     "  if (is_fuchsia) {\n"
     '    sources += [ "os_metrics_fuchsia.cc" ]',
     "  if (is_haiku) {\n"
     '    sources += [ "os_metrics_haiku.cc" ]\n'
     "  }\n"
     "\n"
     "  if (is_fuchsia) {\n"
     '    sources += [ "os_metrics_fuchsia.cc" ]'),

    # GPU info collection. The Fuchsia collector reports a software device,
    # which is what this port has.
    ("gpu/config/BUILD.gn",
     "  if (is_fuchsia) {\n"
     '    sources += [ "gpu_info_collector_fuchsia.cc" ]',
     "  if (is_fuchsia || is_haiku) {\n"
     '    sources += [ "gpu_info_collector_fuchsia.cc" ]'),
    # animation_linux.cc asks ui::LinuxUi whether animations are wanted, so
    # taking that file means taking the dep too. LinuxUi is the toolkit
    # abstraction, not anything kernel-specific, and its own BUILD.gn
    # already asserts is_linux || is_haiku.
    ("ui/gfx/animation/BUILD.gn",
     "  if (is_linux) {\n"
     '    deps += [ "//ui/linux:linux_ui" ]',
     "  if (is_linux || is_haiku) {\n"
     '    deps += [ "//ui/linux:linux_ui" ]'),

    # The drag-and-drop provider the factory falls back to. Its guard names
    # the platforms rather than the condition, which is "aura without a
    # platform-specific provider" -- Haiku.
    ("ui/base/BUILD.gn",
     "  if (is_chromeos_ash || (use_aura && (is_linux || is_chromeos_lacros)) ||\n"
     "      is_fuchsia) {",
     "  if (is_chromeos_ash || (use_aura && (is_linux || is_chromeos_lacros)) ||\n"
     "      is_fuchsia || is_haiku) {"),
    # Launching child processes. The Linux file is half zygote and namespace
    # sandbox; Haiku has neither, so it gets its own copy with those taken
    # out and every launch a plain fork-and-exec -- the path the Linux file
    # already falls back to under --no-zygote.
    ("content/browser/BUILD.gn",
     "  if (is_linux) {\n"
     '    sources += [ "speech/tts_linux.cc" ]',
     "  if (is_haiku) {\n"
     "    sources += [\n"
     '      "child_process_launcher_helper_haiku.cc",\n'
     "      # Text to speech: the Fuchsia file is the do-nothing\n"
     "      # implementation, which is what Haiku can offer today.\n"
     '      "speech/tts_fuchsia.cc",\n'
     "    ]\n"
     "  }\n"
     "\n"
     "  if (is_linux) {\n"
     '    sources += [ "speech/tts_linux.cc" ]'),

    # The renderer's platform hooks. The Fuchsia file is three empty
    # functions and an EnableSandbox that returns true, which is the honest
    # shape here: there is no sandbox on this port.
    ("content/renderer/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "renderer_main_platform_delegate_linux.cc" ]',
     "  if (is_haiku) {\n"
     '    sources += [ "renderer_main_platform_delegate_fuchsia.cc" ]\n'
     "  }\n"
     "\n"
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "renderer_main_platform_delegate_linux.cc" ]'),

    # Time zone changes. The Fuchsia monitor looked like the do-nothing
    # implementation but pulls in Fuchsia's own headers, so Haiku gets its
    # own -- which really does nothing.
    ("services/device/time_zone_monitor/BUILD.gn",
     "  if (is_fuchsia) {\n"
     '    sources += [ "time_zone_monitor_fuchsia.cc" ]',
     "  if (is_haiku) {\n"
     '    sources += [ "time_zone_monitor_haiku.cc" ]\n'
     "  }\n"
     "\n"
     "  if (is_fuchsia) {\n"
     '    sources += [ "time_zone_monitor_fuchsia.cc" ]'),
    # gfx::Animation asks the desktop whether animations are wanted.
    # animation_linux.cc reads a GTK setting and returns false without one,
    # which is the right answer here -- Haiku's equivalent lives in
    # app_server and nobody has asked it.
    ("ui/gfx/animation/BUILD.gn",
     "  if (is_linux) {\n"
     '    sources += [ "animation_linux.cc" ]',
     "  if (is_linux || is_haiku) {\n"
     '    sources += [ "animation_linux.cc" ]'),
    # Idle detection. idle_linux.cc asks the screensaver over D-Bus and falls
    # back to "not idle" without it; use_dbus is false here, so that is the
    # path Haiku takes. Honest for now: Haiku has its own idle notion in
    # app_server that nobody has wired up.
    ("ui/base/idle/BUILD.gn",
     "  if (is_linux) {\n"
     '    sources += [ "idle_linux.cc" ]\n'
     '    deps += [ "//ui/display" ]',
     "  if (is_linux || is_haiku) {\n"
     '    sources += [ "idle_linux.cc" ]\n'
     '    deps += [ "//ui/display" ]'),

    # The resource bundle's GetNativeImageNamed. The aura/linux one just
    # wraps the bitmap; there is nothing Linux about it.
    ("ui/base/BUILD.gn",
     "  if (use_aura && (is_linux || is_chromeos)) {\n"
     '    sources += [ "resource/resource_bundle_auralinux.cc" ]',
     "  if (use_aura && (is_linux || is_chromeos || is_haiku)) {\n"
     '    sources += [ "resource/resource_bundle_auralinux.cc" ]'),

    # The file-open dialog. Chromium ships a stub for platforms without one,
    # which is exactly where this port is: a Haiku dialog would be a BFilePanel
    # and that is separate work.
    ("ui/shell_dialogs/BUILD.gn",
     "  if (is_chromeos || is_castos || is_cast_android) {\n"
     '    sources += [ "shell_dialog_stub.cc" ]',
     "  if (is_chromeos || is_castos || is_cast_android || is_haiku) {\n"
     '    sources += [ "shell_dialog_stub.cc" ]'),
    # base/nix reaches into xdg_user_dirs, which Linux pulls in through its
    # own deps block. The is_haiku sources block sits above the point where
    # deps exists, so the dependency goes after the whole Linux if/else --
    # splitting that if/else was how the else ended up attached to the wrong
    # condition the first time.
    ("base/BUILD.gn",
     "  } else {\n"
     "    if (!is_android) {\n"
     "      sources -= [\n"
     '        "linux_util.cc",\n'
     '        "linux_util.h",\n'
     "      ]\n"
     "    }\n"
     "  }",
     "  } else {\n"
     "    if (!is_android) {\n"
     "      sources -= [\n"
     '        "linux_util.cc",\n'
     '        "linux_util.h",\n'
     "      ]\n"
     "    }\n"
     "  }\n"
     "\n"
     "  if (is_haiku) {\n"
     "    deps += [\n"
     '      "//base/third_party/xdg_mime",\n'
     '      "//base/third_party/xdg_user_dirs",\n'
     "    ]\n"
     "  }"),
    # net's MIME type lookup goes through the XDG shared-mime database on
    # Linux, and Haiku now builds base/nix for the same reason. The address
    # tracker and network_interfaces_linux beside it read netlink, which
    # Haiku does not have, so only the one file comes across.
    ("net/BUILD.gn",
     "  if (is_linux || is_chromeos || is_android) {\n"
     "    sources += [\n"
     '      "base/address_tracker_linux.cc",',
     "  if (is_haiku) {\n"
     "    sources += [\n"
     '      "base/platform_mime_util_linux.cc",\n'
     # TestRootCerts has one file per trust store. Haiku has no NSS and
     # no system store to consult, so it takes the same do-nothing
     # implementation Fuchsia does, the one the built-in verifier wants.
     '      "cert/test_root_certs_builtin.cc",\n'
     # The generic system_trust_store.cc sends every platform it does not
     # know to DummySystemTrustStore, which trusts nothing; that fails every
     # handshake. Haiku gets its own, reading the one PEM the system ships.
     '      "cert/internal/system_trust_store_haiku.cc",\n'
     "    ]\n"
     "  }\n"
     "\n"
     "  if (is_linux || is_chromeos || is_android) {\n"
     "    sources += [\n"
     '      "base/address_tracker_linux.cc",',
     '      "base/platform_mime_util_linux.cc",\n      "cert/'),

    # angle::GetSystemInfo(). gpu_info_collector_fuchsia.cc calls it and
    # the target that defines it builds no source on Haiku, so the symbol
    # is missing at the final link. The Linux file answers: without libpci
    # and without X11 it returns false on the first line -- no GPU found --
    # and never reaches the /sys and /etc paths that would not be there.
    ("third_party/angle/BUILD.gn",
     "  if (is_android) {\n"
     "    sources += libangle_gpu_info_util_android_sources\n"
     "  }\n",
     "  if (is_android) {\n"
     "    sources += libangle_gpu_info_util_android_sources\n"
     "  }\n"
     "\n"
     "  if (is_haiku) {\n"
     "    sources += libangle_gpu_info_util_linux_sources\n"
     "  }\n",
     "if (is_haiku) {\n    sources += libangle_gpu_info_util_linux_sources"),

    # skia's default font manager. Haiku builds the same bundled fontconfig
    # Linux does, so the same file answers.
    ("skia/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "ext/fontmgr_default_linux.cc" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    sources += [ "ext/fontmgr_default_linux.cc" ]'),

    # media's audio manager. Haiku has its own media_kit and no ALSA or
    # PulseAudio; audio_manager_linux.cc with both switched off compiles to
    # a manager that reports no devices, which is what this port can honestly
    # offer until someone wires up BSoundPlayer.
    ("media/audio/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "linux/audio_manager_linux.cc" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    sources += [ "linux/audio_manager_linux.cc" ]'),

    # V8's OS layer: platform-posix.cc carries everything portable and each
    # system supplies the rest. Haiku gets its own rather than borrowing
    # platform-linux.cc, which reads /proc/self/maps and pokes a perf file.
    ("v8/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    sources += [\n"
     '      "src/base/debug/stack_trace_posix.cc",\n'
     '      "src/base/platform/platform-linux.cc",',
     "  if (is_haiku) {\n"
     "    sources += [\n"
     '      "src/base/debug/stack_trace_posix.cc",\n'
     '      "src/base/platform/platform-haiku.cc",\n'
     "    ]\n"
     "  }\n"
     "\n"
     "  if (is_linux || is_chromeos) {\n"
     "    sources += [\n"
     '      "src/base/debug/stack_trace_posix.cc",\n'
     '      "src/base/platform/platform-linux.cc",'),
    # base's per-OS implementations. Chromium keeps one file per platform
    # for each of these and picks by OS name; Haiku had none, which is
    # where 24 of the undefined symbols came from. base_paths_posix.cc,
    # elf_reader.cc and base/nix are shared with Linux and needed only the
    # name adding -- nix is getenv plus a default path, and on Haiku the
    # XDG_* variables are unset so it falls back. The rest are new Haiku
    # files built on get_system_info, get_team_info, _get_team_usage_info
    # and set_thread_priority. Two are stubs Chromium already ships for
    # platforms that cannot answer: memory_stubs.cc and
    # file_path_watcher_stub.cc.
    ("base/BUILD.gn",
     '    if (is_linux || is_chromeos) {\n      sources += [\n        "base_paths_posix.cc",\n        "debug/elf_reader.cc",\n        "debug/elf_reader.h",\n        "stack_canary_linux.cc",\n        "stack_canary_linux.h",\n      ]\n    }',
     '    if (is_linux || is_chromeos) {\n      sources += [\n        "base_paths_posix.cc",\n        "debug/elf_reader.cc",\n        "debug/elf_reader.h",\n        "stack_canary_linux.cc",\n        "stack_canary_linux.h",\n      ]\n    }\n\n    if (is_haiku) {\n      sources += [\n        "base_paths_posix.cc",\n        "debug/elf_reader.cc",\n        "debug/elf_reader.h",\n        "files/file_path_watcher_stub.cc",\n        "nix/mime_util_xdg.cc",\n        "nix/mime_util_xdg.h",\n        "nix/xdg_util.cc",\n        "nix/xdg_util.h",\n        "process/memory_stubs.cc",\n        "process/process_haiku.cc",\n        "process/process_handle_haiku.cc",\n        "process/process_metrics_haiku.cc",\n        "system/sys_info_haiku.cc",\n        "threading/platform_thread_haiku.cc",\n      ]\n    }'),
    # libjpeg_turbo's assembly decides its own symbol prefix. jsimdext.inc
    # emits bare names when ELF is defined and underscore-prefixed ones
    # otherwise, and the list of systems that define it did not include
    # Haiku -- so libsimd_asm.a exported _jconst_fancy_upsample_sse2 while
    # the C code asked for jconst_fancy_upsample_sse2. That was 178 of the
    # 261 undefined symbols at the final link, from one missing name in one
    # condition. nasm was already being told -felf32 by nasm_assemble.gni,
    # which keys off is_posix; only this define was left behind.
    ("third_party/libjpeg_turbo/BUILD.gn",
     "    } else if (is_linux || is_android || is_fuchsia || is_chromeos) {\n"
     '      defines += [ "ELF" ]',
     "    } else if (is_linux || is_android || is_fuchsia || is_chromeos ||\n"
     "               is_haiku) {\n"
     '      defines += [ "ELF" ]'),
    # libnetwork is where Haiku keeps the sockets API -- accept, bind,
    # connect, getpeername, getifaddrs, the resolver, and the in6addr_*
    # constants. Nothing networked links without it, and 26 of the first
    # undefined symbols at the final link were in it.
    ("build/config/BUILD.gn",
     '    libs = [ "ssp_nonshared" ]',
     "    libs = [\n"
     '      "network",\n'
     '      "ssp_nonshared",\n'
     "    ]"),
    # ffmpeg_generated.gni selects its source list by OS, and Haiku matched
    # nothing -- so ffmpeg_c_sources was empty, libffmpeg_internal.a was an
    # empty archive, and all 42 av* symbols were undefined at the final
    # link. This is the same decision already made in ffmpeg_options.gni,
    # where Haiku takes Linux's generated config: the .gni files are the
    # output of running ffmpeg's configure, and Linux's is the one that
    # describes this target.
    ("third_party/ffmpeg/ffmpeg_generated.gni",
     "use_linux_config = is_linux || is_chromeos || is_fuchsia",
     "use_linux_config = is_linux || is_chromeos || is_fuchsia || is_haiku"),
    # ffmpeg links -lrt "for clock_gettime on precise", says the comment --
    # Ubuntu 12.04, where clock_gettime had not yet moved into libc. Haiku
    # has it in libroot and no librt at all, so the final link of
    # content_shell stopped at "cannot find -lrt". -lm and -lz stay: Haiku
    # has both.
    ("third_party/ffmpeg/BUILD.gn",
     "      # librt for clock_gettime on precise\n"
     "      libs += [\n"
     '        "m",\n'
     '        "z",\n'
     '        "rt",\n'
     "      ]",
     "      # librt for clock_gettime on precise\n"
     "      libs += [\n"
     '        "m",\n'
     '        "z",\n'
     "      ]\n"
     "      if (!is_haiku) {\n"
     "        # Haiku has clock_gettime in libroot and ships no librt.\n"
     '        libs += [ "rt" ]\n'
     "      }"),
    # The zygote is a fork server with a namespace sandbox around it, and
    # Haiku has neither. use_zygote_handle is derived from is_posix, which
    # Haiku satisfies -- so content_main_runner_impl.cc compiled the zygote
    # path and called ContentMainDelegate::ZygoteStarting and ZygoteForked,
    # which are declared for Linux and ChromeOS only. The 87 port skipped
    # the zygote too (patch 0076).
    ("content/public/common/zygote/features.gni",
     "use_zygote_handle = is_posix && !is_android && !is_mac",
     "use_zygote_handle = is_posix && !is_android && !is_mac && !is_haiku"),
    # Safe Browsing filters its download-file-type list by platform and
    # Haiku is not one the protocol knows, so gn picked the deliberate
    # "unknown_target_arch" and the generator refused. The list says which
    # extensions to warn about on which platform; Linux's is the one that
    # applies here -- no .exe, no .dmg, no .apk.
    ("components/safe_browsing/content/resources/BUILD.gn",
     '  } else if (is_linux) {\n'
     '    target_arch = "linux"',
     '  } else if (is_linux || is_haiku) {\n'
     '    target_arch = "linux"'),
    # content/common gets fontconfig's include path through this dep, which
    # is grouped with the Linux zygote sandbox support and the setproctitle
    # shim -- neither of which Haiku builds. So it takes the one dep it
    # needs rather than joining the block.
    ("content/common/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    deps += [\n"
     '      ":sandbox_support_linux",\n'
     '      ":set_process_title_linux",\n'
     '      "//third_party/fontconfig",\n'
     "    ]\n"
     "  }",
     "  if (is_linux || is_chromeos) {\n"
     "    deps += [\n"
     '      ":sandbox_support_linux",\n'
     '      ":set_process_title_linux",\n'
     '      "//third_party/fontconfig",\n'
     "    ]\n"
     "  }\n"
     "\n"
     "  if (is_haiku) {\n"
     '    deps += [ "//third_party/fontconfig" ]\n'
     "  }"),
    # fontconfig compiles its paths in and they are Linux's: there is
    # no /etc and no /var on Haiku, and the fonts are elsewhere.
    ("third_party/fontconfig/BUILD.gn",
     '    defines = [\n      "HAVE_CONFIG_H",\n      "FC_CACHEDIR=\\"/var/cache/fontconfig\\"",\n      "FC_TEMPLATEDIR=\\"/usr/share/fontconfig/conf.avail\\"",\n      "FONTCONFIG_PATH=\\"/etc/fonts\\"",\n    ]',
     '    if (is_haiku) {\n      # UNVERIFIED against a running Haiku. If the browser renders no text,\n      # start here: fontconfig finds no fonts at all without a fonts.conf to\n      # read, and FONTCONFIG_PATH is where it looks for one.\n      defines = [\n        "HAVE_CONFIG_H",\n        "FC_CACHEDIR=\\"/boot/home/config/cache/fontconfig\\"",\n        "FC_TEMPLATEDIR=\\"/boot/system/data/fontconfig/conf.avail\\"",\n        "FONTCONFIG_PATH=\\"/boot/system/settings/fonts\\"",\n      ]\n    } else {\n      defines = [\n        "HAVE_CONFIG_H",\n        "FC_CACHEDIR=\\"/var/cache/fontconfig\\"",\n        "FC_TEMPLATEDIR=\\"/usr/share/fontconfig/conf.avail\\"",\n        "FONTCONFIG_PATH=\\"/etc/fonts\\"",\n      ]\n    }'),
    # skia picks its font backend by OS and Haiku matched none of them, so
    # this build had no SkFontMgr at all -- a browser that renders no text.
    # Haiku joins the fontconfig backend, which is the one the 87 port used
    # and the one Chromium carries its own copy of, so nothing has to be
    # installed on the target.
    ("skia/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    sources += [\n"
     '      "//third_party/skia/src/ports/SkFontConfigInterface.cpp",',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     "    sources += [\n"
     '      "//third_party/skia/src/ports/SkFontConfigInterface.cpp",'),
    ("skia/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     "    deps += [\n"
     '      "//third_party/expat",\n'
     '      "//third_party/fontconfig",\n'
     '      "//third_party/icu:icuuc",\n'
     "    ]\n"
     "  }",
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     "    deps += [\n"
     '      "//third_party/expat",\n'
     '      "//third_party/fontconfig",\n'
     '      "//third_party/icu:icuuc",\n'
     "    ]\n"
     "  }"),
    # -fstack-protector emits calls to __stack_chk_fail_local, which on Haiku
    # lives in libssp_nonshared.a rather than in libroot. It is a static
    # archive, so it has to come after the objects that reference it -- and
    # extra_ldflags in the toolchain lands before them, which is why putting
    # it there changed nothing. default_libs ends up in {{libs}}, at the end
    # of the link line, which is where an archive belongs.
    ("build/config/BUILD.gn",
     'config("default_libs") {\n'
     "  if (is_win) {",
     'config("default_libs") {\n'
     "  if (is_haiku) {\n"
     "    # libnetwork is where Haiku keeps the sockets API -- accept, bind,\n"
     "    # connect, getpeername, getifaddrs, the resolver, and the\n"
     "    # in6addr_* constants. Nothing networked links without it, and 26\n"
     "    # of the first undefined symbols were in it.\n"
     '    libs = [\n'
     '      "network",\n'
     '      "ssp_nonshared",\n'
     '    ]\n'
     "  }\n"
     "  if (is_win) {"),
    # Native pixmaps are dmabuf, and Haiku has no dmabuf. These three files
    # are gated on use_ozone, which Haiku satisfies without having any of
    # what they need -- and one of them reaches for the Vulkan headers on
    # the way. The Haiku backend allocates its bitmaps through BBitmap and
    # never asks for a pixmap.
    ("gpu/ipc/common/BUILD.gn",
     "  if (use_ozone) {\n"
     "    sources += [\n"
     '      "gpu_memory_buffer_impl_native_pixmap.cc",',
     "  if (use_ozone && !is_haiku) {\n"
     "    sources += [\n"
     '      "gpu_memory_buffer_impl_native_pixmap.cc",'),
    ("gpu/ipc/service/BUILD.gn",
     "  if (use_ozone) {\n"
     "    sources += [\n"
     '      "gpu_memory_buffer_factory_native_pixmap.cc",',
     "  if (use_ozone && !is_haiku) {\n"
     "    sources += [\n"
     '      "gpu_memory_buffer_factory_native_pixmap.cc",'),
    ("ui/gl/BUILD.gn",
     "    if (is_linux || is_chromeos || use_ozone) {\n"
     "      sources += [\n"
     '        "gl_image_native_pixmap.cc",',
     "    if ((is_linux || is_chromeos || use_ozone) && !is_haiku) {\n"
     "      sources += [\n"
     '        "gl_image_native_pixmap.cc",'),
    # gl_fence_android_native_fence_sync and libsync are the Android fence
    # path, built on every posix that is not Fuchsia or Mac -- which now
    # includes Haiku, where libsync wants <linux/types.h> and the fence
    # extension does not exist. The comment above this block excludes
    # Fuchsia for exactly this reason.
    ("ui/gl/BUILD.gn",
     "    if (is_posix && !is_fuchsia && !is_mac) {",
     "    if (is_posix && !is_fuchsia && !is_mac && !is_haiku) {"),
    # ...and the provider it falls back to has to be compiled. Same edit the
    # 87 port made (patch 0032).
    ("ui/base/BUILD.gn",
     "  if (is_chromeos || (use_aura && is_linux)) {",
     "  if (is_chromeos || (use_aura && (is_linux || is_haiku))) {"),
    # net/dns includes <ifaddrs.h> in two files and is a separate target
    # from component("net"), so it needs the BSD header config of its own.
    ("net/dns/BUILD.gn",
     'source_set("dns") {\n',
     'source_set("dns") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),
    ("net/dns/BUILD.gn",
     'source_set("host_resolver_manager") {\n',
     'source_set("host_resolver_manager") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),
    # net reaches for <ifaddrs.h> in three places -- the interface
    # enumeration and two DNS files. Same treatment as libevent and webrtc:
    # Haiku's BSD headers go on this target's include path and no further.
    ("net/BUILD.gn",
     'component("net") {\n',
     'component("net") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),
    # ffmpeg's generated configuration is picked by current_os, and there is
    # no chromium/config/*/haiku -- those directories are the output of
    # running ffmpeg's configure on each platform, which this port has not
    # done. ChromeOS and Fuchsia already borrow Linux's, and for the same
    # reason: same compiler family, same architecture, and the HAVE_* set
    # that matters to ffmpeg is the POSIX one. Haiku borrows it too rather
    # than carrying a copied directory nobody can regenerate.
    ("third_party/ffmpeg/ffmpeg_options.gni",
     "} else if (is_chromeos || is_fuchsia) {\n"
     '  os_config = "linux"',
     "} else if (is_chromeos || is_fuchsia || is_haiku) {\n"
     '  os_config = "linux"'),
    # The sampling profiler's signal-based stack copier reads linux/futex.h
    # and suspends a thread with a signal. Neither exists here; Haiku joins
    # nacl and apple in not building it.
    ("base/BUILD.gn",
     "    if (!is_nacl && !is_apple) {\n"
     "      sources += [\n"
     '        "profiler/stack_base_address_posix.cc",',
     "    if (!is_nacl && !is_apple && !is_haiku) {\n"
     "      sources += [\n"
     '        "profiler/stack_base_address_posix.cc",'),

    # Two more webrtc targets want <ifaddrs.h>, which Haiku keeps in
    # headers/bsd along with getifaddrs in libbsd.
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("rtc_base") {\n',
     'rtc_library("rtc_base") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("threading") {\n',
     'rtc_library("threading") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'),

    # The crash reporter once more -- crashpad this time, reached through
    # //components/crash/core/app. crashpad's address_types.h ends in
    # "#error Unhandled OS type", and teaching it about Haiku would only
    # move the problem: its client wants a handler process, ptrace and
    # /proc. The 87 port dropped it (patches 0060, 0068, U0007) and this is
    # the same cut. Fuchsia already opts out of exactly this, so the guards
    # gain a second name.
    ("content/shell/BUILD.gn",
     "  if (is_fuchsia) {\n"
     '    deps += [ "//third_party/fuchsia-sdk/sdk/fidl/fuchsia.ui.policy" ]\n'
     "  } else {\n"
     "    deps += [\n"
     '      "//components/crash/content/browser",\n'
     '      "//components/crash/core/app",\n'
     "    ]\n"
     "  }",
     "  if (is_fuchsia) {\n"
     '    deps += [ "//third_party/fuchsia-sdk/sdk/fidl/fuchsia.ui.policy" ]\n'
     "  } else if (!is_haiku) {\n"
     "    deps += [\n"
     '      "//components/crash/content/browser",\n'
     '      "//components/crash/core/app",\n'
     "    ]\n"
     "  }"),

    # The source that subclasses CrashReporterClient goes with them.
    # Compiling it without the library that defines the base class leaves
    # its whole vtable undefined at the final link, which the 87 port found
    # out the slow way. The removal has to sit in content_shell_app, which
    # owns those sources -- gn rejects "sources -=" for an entry the target
    # does not have.
    ("content/shell/BUILD.gn",
     "  if (!is_fuchsia) {\n"
     "    deps += [\n"
     '      "//components/crash/core/app",\n'
     '      "//components/crash/core/app:test_support",\n'
     "    ]\n"
     "  }",
     "  if (!is_fuchsia && !is_haiku) {\n"
     "    deps += [\n"
     '      "//components/crash/core/app",\n'
     '      "//components/crash/core/app:test_support",\n'
     "    ]\n"
     "  }\n"
     "  if (is_haiku) {\n"
     "    sources -= [\n"
     '      "app/shell_crash_reporter_client.cc",\n'
     '      "app/shell_crash_reporter_client.h",\n'
     "    ]\n"
     "  }"),
    # Chromium already knows gcc cannot take ##__VA_ARGS__ in
    # standards-conforming mode -- the comment in this very block says so,
    # and the answer is -std=gnu++17 rather than -std=c++17. Haiku was not
    # in the list of systems that reach it, so it fell through to the
    # hardcoded -std=c++17 further down and V8's DEFINE_PARAMETERS() macros
    # produced 702 "expected identifier before ',' token".
    ("build/config/compiler/BUILD.gn",
     '  if (is_linux || is_chromeos || is_android || (is_nacl && is_clang) ||\n'
     '      current_os == "aix") {',
     '  if (is_linux || is_chromeos || is_android || (is_nacl && is_clang) ||\n'
     '      is_haiku || current_os == "aix") {'),
    # webrtc's net_helpers wants <ifaddrs.h>, which Haiku has in headers/bsd
    # along with the getifaddrs implementation in libbsd. Same treatment as
    # libevent: the directory goes on this target's include path and no
    # further.
    ("third_party/webrtc/rtc_base/BUILD.gn",
     'rtc_library("net_helpers") {\n  sources = [',
     'rtc_library("net_helpers") {\n'
     '  if (is_haiku) {\n'
     '    configs += [ "//build/config/haiku:bsd" ]\n'
     '  }\n'
     '  sources = ['),
    # libevent is the one target that needs Haiku's BSD headers, for
    # sys/queue.h. It gets them through a config rather than through the
    # toolchain's global flags, because headers/bsd also carries a
    # sys/param.h defining ALIGN(p) -- and dav1d defines ALIGN(ll, a).
    ("third_party/libevent/BUILD.gn",
     '    include_dirs = [ "haiku" ]',
     '    include_dirs = [ "haiku" ]\n'
     '    configs += [ "//build/config/haiku:bsd" ]'),
    # The crash reporter is the last thing dragging breakpad's Linux client
    # into the build, through crash_key_lib. Chromium already has the switch
    # for a platform without one -- use_crash_key_stubs, which Fuchsia sets --
    # and it compiles crash_key_stubs.cc instead. That is the whole fix; no
    # dependency needs cutting by hand. breakpad's client is what wanted
    # sys/ucontext.h, linux/limits.h and link.h, none of which Haiku has, and
    # it could not have worked anyway: it reads /proc.
    ("components/crash/core/common/BUILD.gn",
     "  use_crash_key_stubs = is_fuchsia",
     "  use_crash_key_stubs = is_fuchsia || is_haiku"),
    # libxslt picks its config.h directory by OS and has no fallback, so on
    # Haiku the include_dirs line named nothing and 19 translation units
    # could not find config.h. libxml, one directory over, already lists
    # Haiku alongside Linux for exactly this; libxslt just never got the
    # same treatment. Linux's config.h is the right one -- the HAVE_* set it
    # records is POSIX, and the places Haiku differs are not in it.
    ("third_party/libxslt/BUILD.gn",
     "  if (is_linux || is_chromeos) {\n"
     '    sources += [ "linux/config.h" ]',
     "  if (is_linux || is_chromeos || is_haiku) {\n"
     '    sources += [ "linux/config.h" ]'),
    ("third_party/libxslt/BUILD.gn",
     "  if (is_linux || is_chromeos || is_android || is_fuchsia) {\n"
     '    include_dirs = [ "linux" ]',
     "  if (is_linux || is_chromeos || is_android || is_fuchsia || is_haiku) {\n"
     '    include_dirs = [ "linux" ]'),
    # content_shell's data_deps pull in breakpad's dump_syms and
    # minidump_stackwalk on every posix, which includes Haiku now. They are
    # symbol tools, not part of running a browser, and breakpad's client
    # wants link.h, sys/ucontext.h and sys/syscall.h -- none of which Haiku
    # has. The 87 port dropped the crash reporter outright
    # (0068-drop-crash-reporter-client-on-haiku); this is the same call.
    ("content/shell/BUILD.gn",
     "  if (is_posix) {\n"
     "    data_deps += [\n"
     '      "//third_party/breakpad:dump_syms",\n'
     '      "//third_party/breakpad:minidump_stackwalk",\n'
     "    ]\n"
     "  }",
     "  if (is_posix && !is_haiku) {\n"
     "    data_deps += [\n"
     '      "//third_party/breakpad:dump_syms",\n'
     '      "//third_party/breakpad:minidump_stackwalk",\n'
     "    ]\n"
     "  }"),
    # The root BUILD.gn's chromium_builder_perf group names angle_perftests
    # too. Narrowing gn_all left this one, which is why the same error kept
    # coming back after each apparently successful edit: three separate
    # references, found one at a time instead of by grepping for all of them
    # at the start.
    ("BUILD.gn",
     '      "//third_party/angle/src/tests:angle_perftests",\n',
     ""),
    ("third_party/angle/src/tests/restricted_traces/BUILD.gn",
     '    "$angle_root/src/tests:angle_perftests",\n',
     ""),
    # //chrome/test:performance_test_suite is the only thing that names
    # angle_perftests, and it is not built here. Making the target exist
    # instead means following angle's `test()` template into Chromium's own,
    # which has no Haiku arm -- deeper than this is worth for a test suite.
    # Excluding Haiku at the one consumer is smaller and clearer.
    ("chrome/test/BUILD.gn",
     "    if (!is_chromeos_lacros) {\n"
     '      data_deps += [ "//third_party/angle/src/tests:angle_perftests" ]',
     "    if (!is_chromeos_lacros && !is_haiku) {\n"
     '      data_deps += [ "//third_party/angle/src/tests:angle_perftests" ]'),
    ("third_party/breakpad/BUILD.gn",
     "if (is_linux || is_chromeos || is_android) {",
     "if (is_linux || is_chromeos || is_android || is_haiku) {"),
    # angle's test targets, named by //chrome/test:performance_test_suite.
    # The same guard appears twice, for end2end and for white-box.
    # The angle_tests group lists angle_perftests and the white-box perf
    # tests directly, so excluding them at chrome/test is not enough. These
    # are line removals rather than a rewritten block: the group continues
    # with several conditional deps += blocks, and an earlier attempt at
    # restructuring one of those in the root BUILD.gn cut it in the wrong
    # place and left every //chrome reference outside the guard.
    ("third_party/angle/src/tests/BUILD.gn",
     '    ":angle_end2end_tests",\n    ":angle_perftests",',
     '    ":angle_end2end_tests",'),
    ("third_party/angle/src/tests/BUILD.gn",
     '      ":angle_white_box_perftests",\n      ":angle_white_box_tests",',
     '      ":angle_white_box_tests",'),

    ("third_party/angle/src/tests/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_android || is_fuchsia || is_apple) {",
     "if (is_win || is_linux || is_chromeos || is_android || is_fuchsia ||\n"
     "    is_apple || is_haiku) {"),
    # ...and a second variant of the same list, without is_fuchsia, which
    # guards angle_perftests and angle_white_box_perftests.
    ("third_party/angle/src/tests/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_android || is_apple) {",
     "if (is_win || is_linux || is_chromeos || is_android || is_apple ||\n"
     "    is_haiku) {"),
    ("third_party/dawn/src/dawn/common/BUILD.gn",
     "if (is_win || is_linux || is_chromeos || is_mac || is_fuchsia || is_android) {",
     "if (is_win || is_linux || is_chromeos || is_mac || is_fuchsia ||\n"
     "    is_android || is_haiku) {"),
]

done = 0
for edit in edits:
    rel, old, new = edit[0], edit[1], edit[2]
    # An optional marker: a string the file contains if and only if this
    # edit has been applied, whatever shape it was applied in. Edits that
    # insert a block need one. The already-applied test below reads the
    # replacement back out of the text, so the day the replacement changes
    # it stops recognising the block it wrote last time and writes a second
    # one -- which ninja reports as two rules for one object file, a build
    # phase later. A marker names that case instead.
    marker = edit[3] if len(edit) > 3 else None
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing: %s" % rel)
        continue
    if marker is not None:
        if new in s:
            continue
        if marker in s:
            print("  STALE: %s -- the block is there but not in this shape; "
                  "put the tree back before rerunning" % rel)
            continue
    # Whether an edit has already been applied cannot be asked the same way
    # for every edit, and getting it wrong is silent both ways.
    #
    #   removal (the replacement is a fragment of what it replaces): `new` is
    #   a substring of the original too, so `new in s` is true before anything
    #   is done and the edit is skipped forever.
    #
    #   addition (the replacement contains what it replaces, as perfetto's
    #   does): `old` is still there afterwards, so `old in s` is true again
    #   and the edit is applied a second and a third time.
    #
    # Which of the two an edit is can be read off the edit itself.
    if new in old:
        # A removal: the replacement is a fragment of what it replaces, so it
        # is a substring of the original too and "new in s" is true before
        # anything is done. Ask whether the thing being removed is still there.
        already = old not in s
    else:
        # An addition or a substitution. "new in s" is not enough either:
        # liftoff-assembler-ia32.h already contained three "uintptr_t
        # offset_imm" of its own, so the check passed and the fourteen
        # "uint32_t offset_imm" were never touched. Take the replacement out
        # of the text first, then ask whether any unpatched occurrence
        # remains.
        already = old not in s.replace(new, "")
    if already:
        continue
    if old not in s:
        print("  PATTERN NOT FOUND: %s" % rel)
        continue
    # Some guards repeat verbatim in one file; replace them all.
    open(path, "w").write(s.replace(old, new))
    print("  defined on Haiku: %s" % rel)
    done += 1
print("target guards: %d" % done)
