#!/usr/bin/env python3
"""The 2 MB data pipe, and why Haiku cannot have it.

`network::URLLoader::ContinueOnResponseStarted()` creates one mojo data pipe
per response before handing the body to the renderer. Its capacity comes from
`GetDataPipeDefaultAllocationSize(kLargerSizeIfPossible)`, which is **2 MB**
on any machine reporting more than 512 MB of RAM. A mojo data pipe is a shared
memory region, which on this platform is a file descriptor pair and a Haiku
area, mapped into a 32-bit address space that already holds a 275 MB binary.

x.com's Vite build fetches on the order of a hundred ES modules at once. A
hundred pipes at 2 MB is 200 MB of shared memory asked for in one burst, and
on a 2 GB machine with 500 MB free the allocation starts failing. There is no
retry: `CreateDataPipe` failing is reported as ERR_INSUFFICIENT_RESOURCES, the
module never arrives, the app never starts, and the window stays white. It is
intermittent because it depends on how much of the burst lands at once.

ChromeOS already takes the 512 KB default for the same reason -- its comment
says "experiences a much higher OOM crash rate if the larger data pipe size is
used" (crbug.com/1306998). Haiku on a VAIO is that argument with smaller
numbers, so it joins that arm.

The logging is the other half. Upstream says nothing when the pipe cannot be
created, which is why this took a netlog to find: the request had already
succeeded, so there was no failed request to look at. One line at the failure
naming the capacity and the descriptor limit would have said it immediately.
"""
import sys

root = sys.argv[1]
done = 0

# 1. Haiku joins the ChromeOS arm.
path = "%s/services/network/public/cpp/features.cc" % root
s = open(path).read()
if "BUILDFLAG(IS_HAIKU)" in s and "kDefaultDataPipeAllocationSize;\n#else" in s:
    print("  features.cc: 이미 적용")
else:
    old = ("#if BUILDFLAG(IS_CHROMEOS)\n"
           "  // TODO(crbug.com/1306998): ChromeOS experiences a much higher OOM crash\n"
           "  // rate if the larger data pipe size is used.\n"
           "  return kDefaultDataPipeAllocationSize;\n"
           "#else")
    assert s.count(old) == 1, s.count(old)
    new = ("#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_HAIKU)\n"
           "  // TODO(crbug.com/1306998): ChromeOS experiences a much higher OOM crash\n"
           "  // rate if the larger data pipe size is used.\n"
           "  //\n"
           "  // Haiku is here for the same reason with smaller numbers. The larger\n"
           "  // pipe is 2 MB and a mojo data pipe is a shared memory region: a\n"
           "  // descriptor pair and an area, mapped into a 32-bit address space that\n"
           "  // already holds a 275 MB binary. A page that fetches a hundred ES\n"
           "  // modules at once -- x.com's Vite build does -- asks for 200 MB in one\n"
           "  // burst, and on a 2 GB machine the allocations start failing. There is\n"
           "  // no retry: the pipe not being created is ERR_INSUFFICIENT_RESOURCES,\n"
           "  // the module never arrives and the window stays white.\n"
           "  return kDefaultDataPipeAllocationSize;\n"
           "#else")
    open(path, "w").write(s.replace(old, new, 1))
    print("  features.cc: Haiku는 512 KB 파이프")
    done += 1

# 2. Say so when it still fails.
path = "%s/services/network/url_loader.cc" % root
s = open(path).read()
if "HAIKU: CreateDataPipe" in s:
    print("  url_loader.cc: 이미 적용")
else:
    old = ("  MojoResult result =\n"
           "      mojo::CreateDataPipe(&options, response_body_stream_, consumer_handle_);\n"
           "  if (result != MOJO_RESULT_OK) {\n"
           "    NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);\n"
           "    return;\n"
           "  }")
    assert s.count(old) == 1, s.count(old)
    new = ("  MojoResult result =\n"
           "      mojo::CreateDataPipe(&options, response_body_stream_, consumer_handle_);\n"
           "  if (result != MOJO_RESULT_OK) {\n"
           "    // Upstream says nothing here, and the silence is expensive: the request\n"
           "    // itself succeeded, so a net log has no failed request to show and the\n"
           "    // only visible symptom is a blank page. Name the capacity and the\n"
           "    // descriptor limit, which are the two things that run out.\n"
           "    struct rlimit nofile = {0, 0};\n"
           "    getrlimit(RLIMIT_NOFILE, &nofile);\n"
           '    LOG(ERROR) << "CreateDataPipe failed: result=" << result\n'
           '               << " capacity=" << options.capacity_num_bytes\n'
           '               << " errno=" << errno\n'
           '               << " RLIMIT_NOFILE=" << (long)nofile.rlim_cur << "/"\n'
           "               << (long)nofile.rlim_max;\n"
           "    NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);\n"
           "    return;\n"
           "  }")
    s = s.replace(old, new, 1)
    if "#include <sys/resource.h>" not in s:
        anchor = '#include "services/network/url_loader.h"'
        assert s.count(anchor) == 1
        s = s.replace(anchor,
                      anchor + "\n\n#if BUILDFLAG(IS_POSIX)\n#include <errno.h>\n"
                      "#include <sys/resource.h>\n#endif", 1)
    open(path, "w").write(s)
    print("  url_loader.cc: 실패를 로그로 남김")
    done += 1

print("datapipe: %d" % done)
