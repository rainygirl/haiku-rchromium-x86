// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <OS.h>

#include "base/files/file_path.h"

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  team_info info;
  if (get_team_info(process, &info) != B_OK)
    return -1;
  return info.parent;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  // team_info::args is the command line the team was started with, and its
  // first word is the executable. Haiku can enumerate the images of the
  // *current* team precisely (get_next_image_info) but not another team's
  // without the debugger interface, and every caller treats an empty path as
  // "do not know".
  team_info info;
  if (get_team_info(process, &info) != B_OK)
    return FilePath();
  std::string args(info.args);
  const size_t space = args.find(' ');
  return FilePath(space == std::string::npos ? args : args.substr(0, space));
}

}  // namespace base
