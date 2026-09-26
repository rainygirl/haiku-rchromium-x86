// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE.md file.
//
// The pieces of V8's OS layer that platform-posix.cc leaves to each system.
// platform-linux.cc answers them with /proc/self/maps and a file perf
// records; Haiku has the kernel's image list and no perf.

#include <OS.h>
#include <image.h>

#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  // No /proc/self/maps here. The kernel lists the images loaded into the team
  // instead, which is the same information in a different shape. V8 uses this
  // only to attribute profiler samples to a library.
  std::vector<SharedLibraryAddress> result;
  image_info info;
  int32 cookie = 0;
  while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(info.text);
    result.push_back(SharedLibraryAddress(
        info.name, start, start + static_cast<uintptr_t>(info.text_size)));
  }
  return result;
}

void OS::SignalCodeMovingGC() {
  // On Linux this touches a file perf is recording, so that a profile taken
  // across a code-moving GC can be split at the right point. There is no perf
  // on Haiku and nothing to signal.
}

void OS::AdjustSchedulingParams() {}

}  // namespace base
}  // namespace v8
