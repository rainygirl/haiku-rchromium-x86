// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Haiku reports per-team CPU time through _get_team_usage_info(), which is
// what /proc/<pid>/stat is read for on Linux.

#include "base/process/process_metrics.h"

#include <OS.h>

#include <memory>

#include "base/memory/ptr_util.h"

namespace base {

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process) {}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  team_usage_info usage;
  if (get_team_usage_info(process_, B_TEAM_USAGE_SELF, &usage) != B_OK)
    return TimeDelta();
  // Both fields are bigtime_t: microseconds.
  return Microseconds(usage.user_time + usage.kernel_time);
}

size_t GetSystemCommitCharge() {
  system_info info;
  if (get_system_info(&info) != B_OK)
    return 0;
  return static_cast<size_t>(info.used_pages) * (B_PAGE_SIZE / 1024);
}

}  // namespace base
