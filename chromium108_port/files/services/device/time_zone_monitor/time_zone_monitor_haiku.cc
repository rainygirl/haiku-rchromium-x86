// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A TimeZoneMonitor that never fires. Haiku does notify about time zone
// changes -- there is a BMessage for it -- but nothing in this port listens
// yet, and the Fuchsia file that would otherwise serve as the do-nothing
// implementation pulls in Fuchsia's own headers.
//
// The consequence is small and worth naming: a page left open across a time
// zone change keeps the old zone until it is reloaded.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace device {

namespace {

class TimeZoneMonitorHaiku : public TimeZoneMonitor {
 public:
  TimeZoneMonitorHaiku() = default;

  TimeZoneMonitorHaiku(const TimeZoneMonitorHaiku&) = delete;
  TimeZoneMonitorHaiku& operator=(const TimeZoneMonitorHaiku&) = delete;

  ~TimeZoneMonitorHaiku() override = default;
};

}  // namespace

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorHaiku>();
}

}  // namespace device
