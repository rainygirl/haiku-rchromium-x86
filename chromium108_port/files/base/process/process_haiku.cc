// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The parts of Process that Linux answers through /proc and cgroups. Haiku
// has get_team_info for the first and nothing for the second.

#include "base/process/process.h"

#include <OS.h>

#include "base/process/internal_linux.h"
#include "base/time/time.h"

namespace base {

Time Process::CreationTime() const {
  // Haiku's team_info carries no start time, and there is no /proc to read
  // one from. Callers use this for ordering and for about:memory; an empty
  // Time is the documented "unknown".
  return Time();
}

bool Process::CanBackgroundProcesses() {
  return false;
}

bool Process::IsProcessBackgrounded() const {
  return false;
}

bool Process::SetProcessBackgrounded(bool background) {
  // Haiku has per-thread priorities but no notion of backgrounding a whole
  // team, and set_thread_priority on somebody else's threads is not
  // something this port should be doing.
  return false;
}

}  // namespace base
