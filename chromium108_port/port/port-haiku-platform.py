#!/usr/bin/env python3
"""Haiku substitutes for calls that do not exist here.

Everything in build-config land is about telling Chromium which OS it is
on. This file is the other half: the places where, having been told, the
code reaches for a function Haiku does not have and something has to be put
in its place.
"""
import sys

root = sys.argv[1]

# Haiku's answer to "where does this thread's stack end", in both places that
# ask. pthread_getattr_np is a glibc extension; Haiku does not have it, and
# has never needed to, because get_thread_info() has carried stack_base and
# stack_end since BeOS. stack_end is the high address, which is what both
# callers want: they compute base + size to get there.
HAIKU_STACK_TOP = '''#elif defined(__HAIKU__)

void* GetStackTop() {
  thread_info info;
  if (get_thread_info(find_thread(nullptr), &info) != B_OK)
    return nullptr;
  return info.stack_end;
}

'''

edits = [
    # The same guard as in base/files/file.h, one file over: the systems
    # whose plain stat() is already the large-file one. Haiku is another.
    ("base/files/file_posix.cc",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \\\n"
     "    BUILDFLAG(IS_FUCHSIA) || (BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21)",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \\\n"
     "    BUILDFLAG(IS_HAIKU) || \\\n"
     "    BUILDFLAG(IS_FUCHSIA) || (BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21)"),
    # Closes the #if opened above the mallinfo() body. Without it the file
    # ends inside a conditional -- "unterminated #if" -- and every member
    # definition after this function lands outside its namespace.
    ("base/trace_event/malloc_dump_provider.cc",
     "                              total_allocated_size);\n"
     "  }\n"
     "}\n"
     "#endif",
     "                              total_allocated_size);\n"
     "  }\n"
     "#endif  // defined(__HAIKU__)\n"
     "}\n"
     "#endif"),

    # ftruncate64, stat64, fstat64 and lstat64 are the 32-bit-off_t systems'
    # large-file entry points. Haiku's off_t is 64-bit and the plain calls
    # are the large-file ones, which is the same reason the BSDs, Apple and
    # Fuchsia are already excluded here.
    ("base/files/file_posix.cc",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)\n"
     "  static_assert(sizeof(off_t) >= sizeof(int64_t),",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA) || \\\n"
     "    BUILDFLAG(IS_HAIKU)\n"
     "  static_assert(sizeof(off_t) >= sizeof(int64_t),"),

    # SystemMemoryInfoKB is declared only for the systems that can fill it
    # in, and process_metrics.cc defines its constructors unconditionally.
    # Haiku can report total and free memory through get_system_info(), so
    # the struct belongs; the fields it cannot fill stay at their defaults.
    ("base/process/process_metrics.h",
     "#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||      \\\n"
     "    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_AIX) || \\\n"
     "    BUILDFLAG(IS_FUCHSIA)",
     "#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||      \\\n"
     "    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_AIX) || \\\n"
     "    BUILDFLAG(IS_HAIKU) || BUILDFLAG(IS_FUCHSIA)"),

    # IP_MTU_DISCOVER is Linux's path-MTU control and IPV6_TCLASS its IPv6
    # DSCP field. The file already has an arm for systems with neither --
    # macOS, the BSDs, Native Client -- which logs and returns -1.
    ("third_party/webrtc/rtc_base/physical_socket_server.cc",
     "#elif defined(WEBRTC_MAC) || defined(BSD) || defined(__native_client__)\n"
     "      RTC_LOG(LS_WARNING) << \"Socket::OPT_DONTFRAGMENT not supported.\";",
     "#elif defined(WEBRTC_MAC) || defined(BSD) || defined(__native_client__) || \\\n"
     "    defined(__HAIKU__)\n"
     "      RTC_LOG(LS_WARNING) << \"Socket::OPT_DONTFRAGMENT not supported.\";"),
    ("third_party/webrtc/rtc_base/physical_socket_server.cc",
     "    case OPT_DSCP:\n"
     "#if defined(WEBRTC_POSIX)\n"
     "      if (family_ == AF_INET6) {\n"
     "        *slevel = IPPROTO_IPV6;\n"
     "        *sopt = IPV6_TCLASS;\n"
     "      } else {",
     "    case OPT_DSCP:\n"
     "#if defined(WEBRTC_POSIX)\n"
     "      if (family_ == AF_INET6) {\n"
     "#if defined(__HAIKU__)\n"
     "        // No IPV6_TCLASS here, so there is no IPv6 DSCP to set.\n"
     "        return -1;\n"
     "#else\n"
     "        *slevel = IPPROTO_IPV6;\n"
     "        *sopt = IPV6_TCLASS;\n"
     "#endif\n"
     "      } else {"),
    # perfetto's PosixSharedMemory is declared only for the systems it was
    # written for, and system_tracing_backend.cc uses it on every non-Windows
    # build. Haiku goes in the declaration's list: the implementation asks
    # for a memfd first and falls back to an ordinary file when it cannot
    # have one, which is the path Haiku takes.
    ("third_party/perfetto/src/tracing/ipc/posix_shared_memory.h",
     "#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE) || \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)",
     "#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE) || \\\n"
     "    PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA) || defined(__HAIKU__)"),

    # The clock snapshot lists CLOCK_BOOTTIME, CLOCK_REALTIME_COARSE,
    # CLOCK_MONOTONIC_COARSE and CLOCK_MONOTONIC_RAW. Haiku has none of the
    # four -- it has CLOCK_MONOTONIC and CLOCK_REALTIME and that is the set.
    # Rather than snapshot a subset under a different name, Haiku joins the
    # systems that take no snapshot at all.
    ("third_party/perfetto/src/tracing/core/tracing_service_impl.cc",
     "#if !PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE) && \\\n"
     "    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) &&   \\\n"
     "    !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)",
     "#if !PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE) && \\\n"
     "    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) &&   \\\n"
     "    !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL) && !defined(__HAIKU__)"),

    # mincore() again, in a test helper this time. NaCl is already here for
    # the same reason.
    ("third_party/perfetto/src/base/test/vm_test_utils.cc",
     "#elif PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)\n"
     "  // mincore isn't available on NaCL.\n"
     "  ignore_result(page_size);\n"
     "  return true;",
     "#elif PERFETTO_BUILDFLAG(PERFETTO_OS_NACL) || defined(__HAIKU__)\n"
     "  // mincore isn't available on NaCL, nor on Haiku.\n"
     "  ignore_result(page_size);\n"
     "  return true;"),
    # Haiku has no native pixmap handle -- no dmabuf fd, no VMO -- so there
    # is nothing to duplicate. The geometry is still worth copying: the
    # clone stays a valid description of the plane, it just carries no
    # handle, which is what a system with no GPU buffer sharing can say.
    ("ui/gfx/native_pixmap_handle.cc",
     "#else\n#error Unsupported OS\n#endif",
     "#elif BUILDFLAG(IS_HAIKU)\n"
     "    NativePixmapPlane cloned_plane;\n"
     "    cloned_plane.stride = plane.stride;\n"
     "    cloned_plane.offset = plane.offset;\n"
     "    cloned_plane.size = plane.size;\n"
     "    clone.planes.push_back(std::move(cloned_plane));\n"
     "#else\n#error Unsupported OS\n#endif"),
    # V8's ia32 Liftoff takes uint32_t offset_imm where the header declares
    # uintptr_t. On Linux x86-32 those are the same type and nobody noticed;
    # on Haiku uintptr_t is unsigned long and they are not, so eleven
    # definitions stopped matching their declarations. The header is right --
    # the parameter is a pointer offset -- so the ia32 file follows it.
    ("v8/src/wasm/baseline/ia32/liftoff-assembler-ia32.h",
     "uint32_t offset_imm",
     "uintptr_t offset_imm",
     "all"),

    # <ucontext.h> does not exist here. The file already knows one system
    # where it does not -- OpenBSD, whose comment says ucontext_t lives in
    # <signal.h> instead -- and Haiku is the same shape of exception.
    ("v8/src/libsampler/sampler.cc",
     "#elif !V8_OS_OPENBSD\n#include <ucontext.h>",
     "#elif !V8_OS_OPENBSD && !V8_OS_HAIKU\n#include <ucontext.h>"),

    # futimes is absent, futimens is present. The branch that prefers
    # futimens is guarded on __USE_XOPEN2K8, which is a glibc feature macro
    # and not something Haiku defines even though it has the function.
    ("base/files/file_posix.cc",
     "#ifdef __USE_XOPEN2K8\n",
     "#if defined(__USE_XOPEN2K8) || defined(__HAIKU__)\n"),

    # RLIMIT_NICE is a Linux resource limit; Haiku has NZERO but no way to
    # ask how far niceness may be lowered. Answering "no" is the honest
    # answer and the safe one -- the caller falls back to not lowering.
    ("base/posix/can_lower_nice_to.cc",
     "  struct rlimit rlim;\n"
     "  if (getrlimit(RLIMIT_NICE, &rlim) != 0)\n"
     "    return false;",
     "#if defined(__HAIKU__)\n"
     "  // Haiku has no RLIMIT_NICE, so there is nothing to read the allowance\n"
     "  // from. Say no rather than guess.\n"
     "  return false;\n"
     "#else\n"
     "  struct rlimit rlim;\n"
     "  if (getrlimit(RLIMIT_NICE, &rlim) != 0)\n"
     "    return false;"),
    ("base/posix/can_lower_nice_to.cc",
     "  return nice_value >= lowest_nice_allowed;\n}",
     "  return nice_value >= lowest_nice_allowed;\n#endif\n}"),

    # SO_PASSCRED and SCM_CREDENTIALS are Linux's credential passing.
    # macOS is already excluded from both, with an #else that returns true
    # and a conditional that is simply skipped; Haiku joins it.
    ("base/posix/unix_domain_socket.cc",
     "#if !BUILDFLAG(IS_APPLE)",
     "#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_HAIKU)",
     "all"),
    ("base/posix/unix_domain_socket.cc",
     "#endif  // !BUILDFLAG(IS_APPLE)",
     "#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_HAIKU)",
     "all"),

    # Haiku's per-process file descriptors are at /dev/fd, as on the BSDs
    # and Solaris. There is no /proc.
    ("base/process/launch_posix.cc",
     '#elif BUILDFLAG(IS_SOLARIS)\nstatic const char kFDDir[] = "/dev/fd";',
     '#elif BUILDFLAG(IS_HAIKU)\nstatic const char kFDDir[] = "/dev/fd";\n'
     '#elif BUILDFLAG(IS_SOLARIS)\nstatic const char kFDDir[] = "/dev/fd";'),

    # Haiku's default RLIMIT_NOFILE is 256, and this constant is only the
    # guess used when getrlimit itself fails.
    ("base/process/process_metrics_posix.cc",
     "#elif BUILDFLAG(IS_SOLARIS)\nstatic const rlim_t kSystemDefaultMaxFds = 8192;",
     "#elif BUILDFLAG(IS_HAIKU)\nstatic const rlim_t kSystemDefaultMaxFds = 256;\n"
     "#elif BUILDFLAG(IS_SOLARIS)\nstatic const rlim_t kSystemDefaultMaxFds = 8192;"),

    # timegm is in libbsd, which is linked; only the declaration is missing,
    # because headers/bsd is deliberately off the general include path.
    ("base/time/time_exploded_posix.cc",
     "#else  // MacOS (and iOS 64-bit), Linux/ChromeOS, or any other POSIX-compliant.\n"
     "\n"
     "typedef time_t SysTime;\n",
     "#else  // MacOS (and iOS 64-bit), Linux/ChromeOS, or any other POSIX-compliant.\n"
     "\n"
     "#if defined(__HAIKU__)\n"
     "extern \"C\" time_t timegm(struct tm*);\n"
     "#endif\n"
     "\n"
     "typedef time_t SysTime;\n"),

    # mallinfo() is glibc's. Haiku's allocator does not offer an equivalent,
    # so there is nothing for this dump provider to report.
    ("base/trace_event/malloc_dump_provider.cc",
     "#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)\n"
     "#if __GLIBC_PREREQ(2, 33)",
     "#if defined(__HAIKU__)\n"
     "  // Haiku's allocator has no mallinfo() or equivalent; nothing to add.\n"
     "  return;\n"
     "#else\n"
     "#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)\n"
     "#if __GLIBC_PREREQ(2, 33)"),

    # PF_X is the ELF program-header executable flag, value 1 by the ELF
    # specification. Haiku's elf.h does not spell it out.
    ("base/profiler/module_cache_posix.cc",
     "size_t GetLastExecutableOffset(const void* module_addr) {",
     "#if defined(__HAIKU__) && !defined(PF_X)\n"
     "// Not in Haiku's elf.h. 1 by the ELF specification, everywhere.\n"
     "#define PF_X 1\n"
     "#endif\n"
     "\n"
     "size_t GetLastExecutableOffset(const void* module_addr) {"),

    # mincore() asks which pages of a mapping are resident. Haiku has no
    # equivalent. For MADV_FREE discardable memory, "resident" is the
    # conservative answer: it means the pages have not been reclaimed, so
    # the caller keeps treating the block as live rather than as discarded.
    ("base/memory/madv_free_discardable_memory_posix.cc",
     "  int retval =\n"
     "      mincore(data_, allocated_pages_ * base::GetPageSize(), vec.data());",
     "#if defined(__HAIKU__)\n"
     "  // No mincore() here. Report resident, which is the answer that keeps\n"
     "  // the block treated as live.\n"
     "  return true;\n"
     "#else\n"
     "  int retval =\n"
     "      mincore(data_, allocated_pages_ * base::GetPageSize(), vec.data());"),
    ("base/memory/madv_free_discardable_memory_posix.cc",
     "  for (size_t i = 0; i < allocated_pages_; ++i) {\n"
     "    if (!(vec[i] & 1))\n"
     "      return false;\n"
     "  }\n"
     "  return true;\n"
     "}",
     "  for (size_t i = 0; i < allocated_pages_; ++i) {\n"
     "    if (!(vec[i] & 1))\n"
     "      return false;\n"
     "  }\n"
     "  return true;\n"
     "#endif\n"
     "}"),

    # Same missing call, different caller: counting resident bytes for a
    # memory dump. Here there is already a failure path, so use it -- the
    # number is reported as unavailable rather than invented.
    ("base/trace_event/process_memory_dump.cc",
     "#elif BUILDFLAG(IS_POSIX)\n"
     "    int error_counter = 0;\n"
     "    int result = 0;",
     "#elif defined(__HAIKU__)\n"
     "    // Haiku has no mincore(); the count is simply not available.\n"
     "    failure = true;\n"
     "#elif BUILDFLAG(IS_POSIX)\n"
     "    int error_counter = 0;\n"
     "    int result = 0;"),

    # skia's platform canvas declares CreatePlatformCanvasWithPixels by OS
    # name and Haiku was not in the list, so the two inline functions below
    # called something that had never been declared.
    ("skia/ext/platform_canvas.h",
     "#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \\\n"
     "    defined(__sun) || defined(ANDROID) || defined(__APPLE__) ||             \\\n"
     "    defined(__Fuchsia__)",
     "#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \\\n"
     "    defined(__sun) || defined(ANDROID) || defined(__APPLE__) ||             \\\n"
     "    defined(__Fuchsia__) || defined(__HAIKU__)"),

    # IFF_RUNNING is not in Haiku's net/if.h. IFF_UP is the flag it does
    # have, and for the purpose here -- skipping interfaces that are down --
    # it is the right question.
    ("third_party/webrtc/rtc_base/network.cc",
     "    if (!(cursor->ifa_flags & IFF_RUNNING)) {",
     "#if defined(__HAIKU__)\n"
     "    // No IFF_RUNNING here; IFF_UP is what Haiku reports.\n"
     "    if (!(cursor->ifa_flags & IFF_UP)) {\n"
     "#else\n"
     "    if (!(cursor->ifa_flags & IFF_RUNNING)) {\n"
     "#endif"),

    # SIOCGSTAMP is a Linux ioctl for the timestamp of the last packet. The
    # file already has an arm for systems without it -- macOS and Native
    # Client -- which returns -1.
    ("third_party/webrtc/rtc_base/physical_socket_server.cc",
     "#if defined(WEBRTC_POSIX) && !defined(WEBRTC_MAC) && !defined(__native_client__)",
     "#if defined(WEBRTC_POSIX) && !defined(WEBRTC_MAC) && \\\n"
     "    !defined(__native_client__) && !defined(__HAIKU__)"),

    # IPV6_TCLASS likewise. Without it there is no dual-stack DSCP to mirror.
    ("third_party/webrtc/rtc_base/physical_socket_server.cc",
     "#if defined(WEBRTC_POSIX)\n"
     "  if (sopt == IPV6_TCLASS) {",
     "#if defined(WEBRTC_POSIX) && !defined(__HAIKU__)\n"
     "  if (sopt == IPV6_TCLASS) {"),
    # MessagePumpForUI is chosen by OS and the chain ends in an #error.
    # Haiku takes MessagePumpLibevent, the same one Linux without GLib and
    # the BSDs take. libevent already builds here.
    ("base/message_loop/message_pump_for_ui.h",
     "#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_BSD)\n"
     "using MessagePumpForUI = MessagePumpLibevent;",
     "#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \\\n"
     "    BUILDFLAG(IS_BSD) || BUILDFLAG(IS_HAIKU)\n"
     "using MessagePumpForUI = MessagePumpLibevent;"),

    # stat_wrapper_t is "struct stat64" on any POSIX that is not one of the
    # listed exceptions. Haiku has no stat64 and needs none: its off_t is
    # 64-bit and struct stat is the large-file struct. It joins the BSDs.
    ("base/files/file.h",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \\\n"
     "    BUILDFLAG(IS_FUCHSIA) || (BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21)",
     "#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \\\n"
     "    BUILDFLAG(IS_HAIKU) || \\\n"
     "    BUILDFLAG(IS_FUCHSIA) || (BUILDFLAG(IS_ANDROID) && __ANDROID_API__ < 21)"),

    # The third copy of export_template.h, this one webrtc's, failing the
    # same self-test under gcc 13 for the same reason.
    ("third_party/webrtc/rtc_base/system/rtc_export_template.h",
     "RTC_EXPORT_TEMPLATE_TEST(DEFAULT, );  // NOLINT\n"
     'RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __attribute__((visibility("default"))));\n'
     "RTC_EXPORT_TEMPLATE_TEST(MSVC_HACK, __declspec(dllexport));\n"
     "RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));",
     "#if !defined(__HAIKU__)\n"
     "RTC_EXPORT_TEMPLATE_TEST(DEFAULT, );  // NOLINT\n"
     'RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __attribute__((visibility("default"))));\n'
     "RTC_EXPORT_TEMPLATE_TEST(MSVC_HACK, __declspec(dllexport));\n"
     "RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));\n"
     "#endif  // !defined(__HAIKU__)"),

    # std::uintptr_t needs <cstdint>, which this header got transitively on
    # glibc and does not here.
    ("base/check_op.h",
     "#include <cstddef>\n#include <string>",
     "#include <cstddef>\n#include <cstdint>\n#include <string>"),

    # V8's sampler includes <sys/syscall.h> everywhere but QNX and AIX.
    ("v8/src/libsampler/sampler.cc",
     "#if !V8_OS_QNX && !V8_OS_AIX\n#include <sys/syscall.h>\n#endif",
     "#if !V8_OS_QNX && !V8_OS_AIX && !V8_OS_HAIKU\n"
     "#include <sys/syscall.h>\n#endif"),

    # The crash reporter, again -- crashpad this time rather than breakpad,
    # and reached through //components/crash/core/app. crashpad's
    # address_types.h ends in "#error Unhandled OS type", and teaching it
    # about Haiku would only move the problem: its client wants a handler
    # process, ptrace and /proc. The 87 port dropped it (patches 0060 and
    # 0068 and upstream U0007) and this is the same cut at the same three
    # places: the deps, the client source that subclasses
    # CrashReporterClient, and the call sites. Fuchsia already opts out of
    # exactly this, so each guard just gains a second name.
    ("content/shell/app/shell_main_delegate.cc",
     "#if !BUILDFLAG(IS_FUCHSIA)",
     "#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_HAIKU)",
     "all"),
    ("content/shell/app/shell_main_delegate.cc",
     "#endif  // !BUILDFLAG(IS_FUCHSIA)",
     "#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_HAIKU)"),
    # keycode_converter builds a native-scancode -> DomCode table and picks
    # the column by OS, ending in "#error Unsupported platform". Haiku takes
    # the usb column, the same one Fuchsia takes, and that is a statement
    # rather than a placeholder: it says this port has no native scancode
    # mapping. It does not need one. The BeAPI backend in the 87 overlay
    # goes through UsLayoutKeyboardCodeToDomCode() and never consults this
    # table, so an xkb or evdev column here would be a mapping that is
    # wrong and also unused.
    ("ui/events/keycodes/dom/keycode_converter.cc",
     "#elif BUILDFLAG(IS_FUCHSIA)\n"
     "#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \\\n"
     "  { usb, usb, code }\n"
     "#else\n"
     "#error Unsupported platform",
     "#elif BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_HAIKU)\n"
     "#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \\\n"
     "  { usb, usb, code }\n"
     "#else\n"
     "#error Unsupported platform"),

    # libphonenumber picks its lock by OS name too, and the fallback is
    # lock_unsafe.h -- a dummy lock holding a const ThreadChecker. With
    # NDEBUG the ThreadChecker is an empty class, and a const object of an
    # empty class with no user-provided constructor cannot be initialized,
    # which is what "uninitialized const member" meant. Haiku has pthreads
    # and belongs in lock_posix.h, which is both correct and thread-safe.
    ("third_party/libphonenumber/dist/cpp/src/phonenumbers/base/synchronization/lock.h",
     "#elif defined(__linux__) || defined(__APPLE__) || defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD)",
     "#elif defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__) || \\\n"
     "    defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD)"),

    # <sys/syscall.h> is included unconditionally in both of these, for
    # __NR_getrandom and for the clone/fork path. Haiku has neither the
    # header nor raw syscall numbers -- its kernel interface is not a
    # syscall table exposed to userland -- and the code that uses them is
    # already behind IS_LINUX checks.
    ("base/rand_util_posix.cc",
     "#include <sys/syscall.h>\n",
     "#if !defined(__HAIKU__)\n#include <sys/syscall.h>\n#endif\n"),
    ("base/process/launch_posix.cc",
     "#include <sys/syscall.h>\n",
     "#if !defined(__HAIKU__)\n#include <sys/syscall.h>\n#endif\n"),
    # Chromium already has a mode for a POSIX without execinfo -- uclibc and
    # AIX are in it -- and it covers the whole file, not just the include.
    # Haiku has no execinfo.h and no backtrace(); it has its own debugger
    # API, which is a separate piece of work. Until then it takes the same
    # road uclibc does.
    ("base/debug/stack_trace_posix.cc",
     "!defined(__UCLIBC__) && !defined(_AIX)",
     "!defined(__UCLIBC__) && !defined(_AIX) && !defined(__HAIKU__)",
     "all"),

    # _PATH_DEVNULL is the only thing logging.cc wants out of <paths.h>, and
    # Haiku keeps paths.h in headers/bsd, which is deliberately not on the
    # general include path. One define is a smaller thing to carry than the
    # whole BSD header directory.
    ("base/logging.cc",
     "#include <paths.h>",
     "#if defined(__HAIKU__)\n"
     "#define _PATH_DEVNULL \"/dev/null\"\n"
     "#else\n"
     "#include <paths.h>\n"
     "#endif"),

    # AtomicWord is intptr_t and Atomic32 is int32_t. On a 32-bit system
    # where intptr_t is long -- Haiku, like Apple and OpenBSD -- those are
    # distinct types of the same width, and the Atomic32 entry points will
    # not take an AtomicWord*. Chromium keeps a compatibility header for
    # exactly this and lists the platforms that need it; Haiku is another.
    ("base/atomicops.h",
     "#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OPENBSD)\n"
     '#include "base/atomicops_internals_atomicword_compat.h"',
     "#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_HAIKU)\n"
     '#include "base/atomicops_internals_atomicword_compat.h"'),

    # webrtc reaches <endian.h> for htobe64 and friends on any POSIX. Haiku
    # has an endian.h, but the one on the include path only settles byte
    # order; the htobe/letoh family lives in the BSD copy. Rather than put
    # that directory on webrtc's path, answer the same way the file already
    # answers for Native Client: with the compiler builtins. x86 is little
    # endian, which is the only case this port has.
    ("third_party/webrtc/rtc_base/byte_order.h",
     "#elif defined(WEBRTC_POSIX)\n#include <endian.h>",
     "#elif defined(__HAIKU__)\n"
     "\n"
     "#define htobe16(v) __builtin_bswap16(v)\n"
     "#define htobe32(v) __builtin_bswap32(v)\n"
     "#define htobe64(v) __builtin_bswap64(v)\n"
     "#define be16toh(v) __builtin_bswap16(v)\n"
     "#define be32toh(v) __builtin_bswap32(v)\n"
     "#define be64toh(v) __builtin_bswap64(v)\n"
     "\n"
     "#define htole16(v) (v)\n"
     "#define htole32(v) (v)\n"
     "#define htole64(v) (v)\n"
     "#define le16toh(v) (v)\n"
     "#define le32toh(v) (v)\n"
     "#define le64toh(v) (v)\n"
     "\n"
     "#elif defined(WEBRTC_POSIX)\n#include <endian.h>"),
    # perfetto asks the OS for a thread id and, with every PERFETTO_OS_*
    # flag at 0, lands in "Default to pthreads in case no OS is set", where
    # PlatformThreadId is pthread_t. On Haiku that is a pointer, so the
    # static_cast to uint32_t that the tracing code does is ill-formed.
    # Haiku's own thread id is a small integer and find_thread(nullptr)
    # returns it.
    ("third_party/perfetto/include/perfetto/base/thread_utils.h",
     "#else\n#include <pthread.h>\n#endif",
     "#elif defined(__HAIKU__)\n#include <OS.h>\n"
     "#else\n#include <pthread.h>\n#endif"),
    ("third_party/perfetto/include/perfetto/base/thread_utils.h",
     "#else  // Default to pthreads in case no OS is set.\n"
     "using PlatformThreadId = pthread_t;",
     "#elif defined(__HAIKU__)\n"
     "using PlatformThreadId = int32_t;\n"
     "inline PlatformThreadId GetThreadId() {\n"
     "  return static_cast<int32_t>(find_thread(nullptr));\n"
     "}\n"
     "#else  // Default to pthreads in case no OS is set.\n"
     "using PlatformThreadId = pthread_t;"),

    # CLOCK_BOOTTIME is a Linux clock and Haiku does not define it. The
    # code already treats it as something that may not work -- it calls
    # clock_gettime and falls back to the wall clock if that fails -- so
    # the fallback just has to be reachable when the constant is missing
    # too. Guarding on the macro rather than on the OS keeps that local.
    ("third_party/perfetto/include/perfetto/base/time.h",
     "  static const clockid_t kBootTimeClockSource = [] {\n"
     "    struct timespec ts = {};\n"
     "    int res = clock_gettime(CLOCK_BOOTTIME, &ts);\n"
     "    return res == 0 ? CLOCK_BOOTTIME : kWallTimeClockSource;\n"
     "  }();",
     "  static const clockid_t kBootTimeClockSource = [] {\n"
     "#if defined(CLOCK_BOOTTIME)\n"
     "    struct timespec ts = {};\n"
     "    int res = clock_gettime(CLOCK_BOOTTIME, &ts);\n"
     "    return res == 0 ? CLOCK_BOOTTIME : kWallTimeClockSource;\n"
     "#else\n"
     "    return kWallTimeClockSource;\n"
     "#endif\n"
     "  }();"),

    # timegm exists on Haiku, in libbsd, declared in headers/bsd/time.h --
    # which is not on the include path any more and should not be, because
    # of the ALIGN collision that put it there. -lbsd is linked globally, so
    # the function is present; only its declaration is missing, and one
    # line supplies that.
    ("third_party/perfetto/include/perfetto/base/time.h",
     "inline int64_t TimeGm(struct tm* tms) {",
     "#if defined(__HAIKU__)\n"
     "extern \"C\" time_t timegm(struct tm*);\n"
     "#endif\n"
     "\n"
     "inline int64_t TimeGm(struct tm* tms) {"),

    # Haiku's struct dirent has no d_type -- it carries the device and inode
    # numbers instead, which is a BeOS inheritance rather than an omission.
    # stat() answers the same question at the cost of a syscall per entry,
    # and this walk is not on any hot path.
    ("third_party/perfetto/src/base/file_utils.cc",
     "      if (dirent->d_type == DT_DIR) {\n"
     "        dir_queue.push_back(cur_dir + dirent->d_name + '/');\n"
     "      } else if (dirent->d_type == DT_REG) {\n"
     "        const std::string full_path = cur_dir + dirent->d_name;\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }",
     "#if defined(__HAIKU__)\n"
     "      const std::string full_path = cur_dir + dirent->d_name;\n"
     "      struct stat entry_stat;\n"
     "      if (stat(full_path.c_str(), &entry_stat) != 0)\n"
     "        continue;\n"
     "      if (S_ISDIR(entry_stat.st_mode)) {\n"
     "        dir_queue.push_back(full_path + '/');\n"
     "      } else if (S_ISREG(entry_stat.st_mode)) {\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }\n"
     "#else\n"
     "      if (dirent->d_type == DT_DIR) {\n"
     "        dir_queue.push_back(cur_dir + dirent->d_name + '/');\n"
     "      } else if (dirent->d_type == DT_REG) {\n"
     "        const std::string full_path = cur_dir + dirent->d_name;\n"
     "        PERFETTO_CHECK(full_path.length() > root_dir_path.length());\n"
     "        output.push_back(full_path.substr(root_dir_path.length()));\n"
     "      }\n"
     "#endif"),

    # libphonenumber picks between a real ThreadChecker and an empty one by
    # OS name. Haiku matched neither, so it got the empty class -- and a
    # class with no members and no user-provided constructor cannot be
    # declared const, which is what "uninitialized const member" means here.
    # Haiku has pthreads; it belongs in the real one.
    ("third_party/libphonenumber/dist/cpp/src/phonenumbers/base/thread_checker.h",
     "    (defined(__linux__) || defined(__APPLE__) || defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD))",
     "    (defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__) || \\\n"
     "     defined(I18N_PHONENUMBERS_HAVE_POSIX_THREAD))"),
    # sys/syscall.h is included for gettid, unconditionally except on AIX
    # and Fuchsia. Haiku has neither the header nor the call; the Haiku arm
    # added to Stack::GetStackStart below is inside the file, which does not
    # help when the file will not preprocess.
    ("v8/src/base/platform/platform-posix.cc",
     "#if !defined(_AIX) && !defined(V8_OS_FUCHSIA)\n"
     "#include <sys/syscall.h>\n"
     "#endif",
     "#if !defined(_AIX) && !defined(V8_OS_FUCHSIA) && !defined(V8_OS_HAIKU)\n"
     "#include <sys/syscall.h>\n"
     "#endif"),
    # partition_alloc's own bug, surfaced by this libstdc++ rather than
    # caused by it. MetadataAllocator::operator== is not const, and the COW
    # std::string in Haiku's gcc 13 compares allocators as
    # `__a == _Alloc()` -- a const lvalue against a temporary. libc++ and the
    # SSO libstdc++ never make that comparison, so upstream never saw it. An
    # allocator's equality operator being const is what the standard asks
    # for anyway.
    ("base/allocator/partition_allocator/starscan/metadata_allocator.h",
     "  bool operator==(const MetadataAllocator<U>&) {",
     "  bool operator==(const MetadataAllocator<U>&) const {"),

    ("base/allocator/partition_allocator/starscan/stack/stack.cc",
     "#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)\n\nvoid* GetStackTop() {",
     HAIKU_STACK_TOP +
     "#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)\n\nvoid* GetStackTop() {"),

    # V8 asks the same question through its own function. Rather than take
    # Haiku out of the guard -- which would leave Stack::GetStackStart
    # undefined at link time -- the Haiku answer goes in at the top of the
    # body and the rest stays where it is.
    ("v8/src/base/platform/platform-posix.cc",
     "Stack::StackSlot Stack::GetStackStart() {\n"
     "  pthread_attr_t attr;\n"
     "  int error = pthread_getattr_np(pthread_self(), &attr);",
     "Stack::StackSlot Stack::GetStackStart() {\n"
     "#if defined(V8_OS_HAIKU)\n"
     "  thread_info info;\n"
     "  if (get_thread_info(find_thread(nullptr), &info) != B_OK)\n"
     "    return nullptr;\n"
     "  return info.stack_end;\n"
     "#else\n"
     "  pthread_attr_t attr;\n"
     "  int error = pthread_getattr_np(pthread_self(), &attr);"),
    ("v8/src/base/platform/platform-posix.cc",
     "#else\n"
     "  return nullptr;\n"
     "#endif  // !defined(V8_LIBC_GLIBC)\n"
     "}\n"
     "\n"
     "#endif  // !defined(V8_OS_FREEBSD) && !defined(V8_OS_DARWIN) &&",
     "#else\n"
     "  return nullptr;\n"
     "#endif  // !defined(V8_LIBC_GLIBC)\n"
     "#endif  // !defined(V8_OS_HAIKU)\n"
     "}\n"
     "\n"
     "#endif  // !defined(V8_OS_FREEBSD) && !defined(V8_OS_DARWIN) &&"),
]

includes = [
    # <OS.h> is where find_thread, get_thread_info and thread_info live.
    ("base/allocator/partition_allocator/starscan/stack/stack.cc",
     '#include "base/allocator/partition_allocator/starscan/stack/stack.h"'),
    ("v8/src/base/platform/platform-posix.cc",
     '#include "src/base/platform/platform-posix.h"'),
]

done = 0
for edit in edits:
    rel, old, new = edit[0], edit[1], edit[2]
    every = len(edit) > 3 and edit[3] == "all"
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        print("  missing: %s" % rel)
        continue
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
    open(path, "w").write(s.replace(old, new) if every else s.replace(old, new, 1))
    print("  patched %s" % rel)
    done += 1

for rel, anchor in includes:
    path = "%s/%s" % (root, rel)
    try:
        s = open(path).read()
    except FileNotFoundError:
        continue
    if "#include <OS.h>" in s:
        continue
    if anchor not in s:
        print("  ANCHOR NOT FOUND: %s" % rel)
        continue
    s = s.replace(anchor,
                  anchor + "\n\n#if defined(__HAIKU__)\n#include <OS.h>\n#endif",
                  1)
    open(path, "w").write(s)
    print("  OS.h in %s" % rel)
    done += 1

print("haiku platform: %d" % done)
