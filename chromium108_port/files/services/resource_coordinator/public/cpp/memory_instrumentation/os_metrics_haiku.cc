// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Per-process memory instrumentation. The Linux implementation parses
// /proc/<pid>/smaps, which Haiku does not have: the kernel reports areas
// through get_next_area_info() instead, and mapping those onto the
// resident/shared/private breakdown smaps gives is more than a link fix.
//
// Reporting nothing is the honest placeholder. about:memory will show this
// process with no detail rather than with invented numbers.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace memory_instrumentation {

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  return false;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessId pid) {
  return std::vector<mojom::VmRegionPtr>();
}

}  // namespace memory_instrumentation
