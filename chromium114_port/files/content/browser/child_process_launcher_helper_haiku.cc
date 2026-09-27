// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Linux file, minus the two things Haiku has not got: the zygote and the
// namespace sandbox. What is left is the path Linux already takes under
// --no-zygote -- a plain fork and exec through base::LaunchProcess.
//
// No sandbox either. The renderer runs with the same rights as the browser.
//
// 108 -> 114: LaunchOptions is passed by pointer now, not by reference, and
// a null pointer means "launching through the zygote". There is no zygote
// here, so it is never null and IsUsingLaunchOptions() is always true.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace content {
namespace internal {

absl::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return absl::nullopt;
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return CreateDefaultPosixFilesToMap(
      child_process_id(), mojo_channel_->remote_endpoint(),
      file_data_->files_to_preload, GetProcessType(), command_line());
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return true;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(options);
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);
  options->environment = delegate_->GetEnvironment();
  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;
  DCHECK(options);
  Process process;
  process.process = base::LaunchProcess(*command_line(), *options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  // Reset any FDs still held open.
  file_data_.reset();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  if (known_dead) {
    info.status = base::GetKnownDeadTerminationStatus(process.process.Handle(),
                                                      &info.exit_code);
  } else {
    info.status =
        base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  }
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  // On POSIX, we must additionally reap the child.
  base::EnsureProcessTerminated(std::move(process.process));
}

void ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread(
    base::Process process,
    bool is_background) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(is_background);
}

base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  base::FilePath exe_dir;
  bool result = base::PathService::Get(base::BasePathKey::DIR_EXE, &exe_dir);
  DCHECK(result);
  base::File file(exe_dir.Append(path),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  *region = base::MemoryMappedFile::Region::kWholeFile;
  return file;
}

}  // namespace internal
}  // namespace content
