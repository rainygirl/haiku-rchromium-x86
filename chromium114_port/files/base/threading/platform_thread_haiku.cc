// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Haiku half of PlatformThread. platform_thread_posix.cc carries
// everything that is pthreads; this file carries the parts Linux answers with
// prctl, sched_setscheduler and a nice-value table, for which Haiku has its
// own calls.

#include "base/threading/platform_thread.h"

#include <OS.h>

#include "base/process/process_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"

namespace base {

namespace internal {

// Haiku's priorities run the other way from nice: higher is more urgent, and
// B_NORMAL_PRIORITY is 10. The table below is still expressed as nice values
// because that is the interface, and SetCurrentThreadTypeForPlatform converts.
//
// The order here matters: increasing priority, so decreasing nice value.
const ThreadTypeToNiceValuePair kThreadTypeToNiceValueMap[7] = {
    {ThreadType::kBackground, 10},      // B_LOW_PRIORITY
    {ThreadType::kUtility, 7},
    {ThreadType::kResourceEfficient, 5},
    {ThreadType::kDefault, 0},          // B_NORMAL_PRIORITY
    {ThreadType::kCompositing, -5},
    {ThreadType::kDisplayCritical, -8}, // B_DISPLAY_PRIORITY
    {ThreadType::kRealtimeAudio, -10},  // B_URGENT_DISPLAY_PRIORITY
};

bool CanSetThreadTypeToRealtimeAudio() {
  // set_thread_priority() needs no privilege on Haiku, so the answer is yes
  // for every thread in the team.
  return true;
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                     MessagePumpType pump_type_hint) {
  int32 priority = B_NORMAL_PRIORITY;
  switch (thread_type) {
    case ThreadType::kBackground:
      priority = B_LOW_PRIORITY;
      break;
    case ThreadType::kResourceEfficient:
      priority = B_NORMAL_PRIORITY - 2;
      break;
    case ThreadType::kDefault:
      priority = B_NORMAL_PRIORITY;
      break;
    case ThreadType::kCompositing:
    case ThreadType::kDisplayCritical:
      priority = B_DISPLAY_PRIORITY;
      break;
    case ThreadType::kRealtimeAudio:
      priority = B_URGENT_DISPLAY_PRIORITY;
      break;
  }
  return set_thread_priority(find_thread(nullptr), priority) >= B_OK;
}

absl::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
  return absl::nullopt;
}

}  // namespace internal

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);
  // Haiku thread names are capped at B_OS_NAME_LENGTH and rename_thread
  // truncates rather than failing, which is what every other platform here
  // does with its own limit.
  rename_thread(find_thread(nullptr), name.c_str());
}

// These three are in base::, not base::internal -- platform_thread_linux.cc
// defines them after closing its internal namespace.
void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  // 0 means "whatever pthreads gives you", which on Haiku is 64 KB of user
  // stack grown on demand up to 16 MB.
  return 0;
}

}  // namespace base
