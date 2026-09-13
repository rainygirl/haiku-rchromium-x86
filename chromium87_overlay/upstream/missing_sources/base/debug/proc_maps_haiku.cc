// Copyright 2026 The R Chromium Native Port authors.
// Use of this source code is governed by a BSD-style license that can be
// found in Chromium's LICENSE file.
//
// Haiku replacement for base/debug/proc_maps_linux.cc.
//
// The HaikuPorts patchset removes proc_maps_linux.cc from base's sources on
// this platform -- correctly, since it parses /proc/self/maps and Haiku has no
// /proc -- but unlike every other file it drops there, it supplies nothing in
// its place. base/debug/stack_trace_posix.cc still compiles its
// SandboxSymbolizeHelper (USE_SYMBOLIZE is on for this build) and calls both
// entry points, so they went undefined at the final content_shell link:
//
//     undefined reference to `base::debug::ReadProcMaps(std::string*)'
//     undefined reference to `base::debug::ParseProcMaps(std::string const&,
//         std::vector<base::debug::MappedMemoryRegion, ...>*)'
//
// Both callers treat a false return as "the maps file is unavailable" and fall
// back to symbolizing without it, which is the truthful answer on Haiku rather
// than a silent lie about the address space. Haiku can enumerate its own areas
// through get_next_area_info(), so a real implementation is possible later;
// nothing in this port needs one yet, and a stub keeps the failure mode honest.

#include "base/debug/proc_maps_linux.h"

namespace base {
namespace debug {

bool ReadProcMaps(std::string* proc_maps) {
  proc_maps->clear();
  return false;
}

bool ParseProcMaps(const std::string& input,
                   std::vector<MappedMemoryRegion>* regions_out) {
  regions_out->clear();
  return false;
}

}  // namespace debug
}  // namespace base
