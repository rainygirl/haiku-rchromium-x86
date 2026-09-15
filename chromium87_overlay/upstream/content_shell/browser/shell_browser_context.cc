// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_context.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_permission_manager.h"
#include "content/shell/common/shell_switches.h"
#include "content/public/browser/background_sync_controller.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/nix/xdg_util.h"
#elif defined(OS_MAC)
#include "base/base_paths_mac.h"
#elif defined(OS_FUCHSIA)
#include "base/base_paths_fuchsia.h"
#elif defined(OS_HAIKU)
// base_paths.h does not pull this in on Haiku (its OS chain covers only
// win/apple/posix-fuchsia), so base::DIR_APP_DATA -- declared here and
// implemented in base_paths_haiku.cc -- is otherwise invisible in this file.
#include "base/base_paths_haiku.h"
#endif

namespace content {

namespace {

// The pinned qtwebengine-chromium distribution omits content/test, which is
// where MockBackgroundSyncController lives. Callers such as BackgroundSyncProxy
// dereference the controller straight after a DCHECK, so returning null is not
// an option in a release build. Background Sync is not a goal of this port, so
// supply an inert controller instead.
class InertBackgroundSyncController : public BackgroundSyncController {
 public:
  InertBackgroundSyncController() = default;
  ~InertBackgroundSyncController() override = default;

  base::TimeDelta GetNextEventDelay(
      const BackgroundSyncRegistration& registration,
      content::BackgroundSyncParameters* parameters,
      base::TimeDelta time_till_soonest_scheduled_event_for_origin) override {
    return base::TimeDelta::Max();
  }
  std::unique_ptr<BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() override {
    return nullptr;
  }
  void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) override {}
  void NoteRegisteredPeriodicSyncOrigins(
      std::set<url::Origin> registered_origins) override {}
  void AddToTrackedOrigins(const url::Origin& origin) override {}
  void RemoveFromTrackedOrigins(const url::Origin& origin) override {}
};

}  // namespace

ShellBrowserContext::ShellResourceContext::ShellResourceContext() {}

ShellBrowserContext::ShellResourceContext::~ShellResourceContext() {
}

ShellBrowserContext::ShellBrowserContext(bool off_the_record,
                                         bool delay_services_creation)
    : resource_context_(std::make_unique<ShellResourceContext>()),
      off_the_record_(off_the_record) {
  InitWhileIOAllowed();
  if (!delay_services_creation) {
    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServices(this);
  }
}

ShellBrowserContext::~ShellBrowserContext() {
  NotifyWillBeDestroyed(this);

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

  // Need to destruct the ResourceContext before posting tasks which may delete
  // the URLRequestContext because ResourceContext's destructor will remove any
  // outstanding request while URLRequestContext's destructor ensures that there
  // are no more outstanding requests.
  if (resource_context_) {
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          resource_context_.release());
  }
  ShutdownStoragePartitions();
}

void ShellBrowserContext::InitWhileIOAllowed() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kIgnoreCertificateErrors))
    ignore_certificate_errors_ = true;
  if (cmd_line->HasSwitch(switches::kContentShellDataPath)) {
    path_ = cmd_line->GetSwitchValuePath(switches::kContentShellDataPath);
    if (base::DirectoryExists(path_) || base::CreateDirectory(path_))  {
      // BrowserContext needs an absolute path, which we would normally get via
      // PathService. In this case, manually ensure the path is absolute.
      if (!path_.IsAbsolute())
        path_ = base::MakeAbsoluteFilePath(path_);
      if (!path_.empty()) {
        FinishInitWhileIOAllowed();
        return;
      }
    } else {
      LOG(WARNING) << "Unable to create data-path directory: " << path_.value();
    }
  }

#if defined(OS_WIN)
  CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path_));
  path_ = path_.Append(std::wstring(L"content_shell"));
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(
      base::nix::GetXDGDirectory(env.get(),
                                 base::nix::kXdgConfigHomeEnvVar,
                                 base::nix::kDotConfigDir));
  path_ = config_dir.Append("content_shell");
#elif defined(OS_MAC)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));
  path_ = path_.Append("Chromium Content Shell");
#elif defined(OS_ANDROID)
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path_));
  path_ = path_.Append(FILE_PATH_LITERAL("content_shell"));
#elif defined(OS_FUCHSIA)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));
  path_ = path_.Append(FILE_PATH_LITERAL("content_shell"));
#elif defined(OS_HAIKU)
  // Without this, path_ stays empty on Haiku (the branches above are all
  // guarded to other OSes and the #else just NOTIMPLEMENTED()s). An empty
  // browser-context path makes StoragePartitionImpl::GetStorageServicePartition
  // call BindPartition() with a path rooted at "", the in-process storage
  // service fails to create a Partition there and closes the pipe, and
  // OnStorageServiceDisconnected -> RecoverFromStorageServiceCrash rebinds and
  // fails again in a tight loop. The renderer then blocks forever on the
  // synchronous localStorage GetAll it issues on navigation, so any page that
  // touches localStorage (news.naver.com) comes up blank. DIR_APP_DATA is
  // B_USER_SETTINGS_DIRECTORY (patch: base_paths_haiku.cc), i.e.
  // ~/config/settings, so this lands at ~/config/settings/content_shell.
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));
  path_ = path_.Append("content_shell");
#else
  NOTIMPLEMENTED();
#endif

  if (!base::PathExists(path_))
    base::CreateDirectory(path_);

  FinishInitWhileIOAllowed();
}

void ShellBrowserContext::FinishInitWhileIOAllowed() {
  key_ = std::make_unique<SimpleFactoryKey>(path_, off_the_record_);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());
}

#if !defined(OS_ANDROID)
std::unique_ptr<ZoomLevelDelegate> ShellBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath&) {
  return std::unique_ptr<ZoomLevelDelegate>();
}
#endif  // !defined(OS_ANDROID)

base::FilePath ShellBrowserContext::GetPath() {
  return path_;
}

bool ShellBrowserContext::IsOffTheRecord() {
  return off_the_record_;
}

DownloadManagerDelegate* ShellBrowserContext::GetDownloadManagerDelegate()  {
  if (!download_manager_delegate_.get()) {
    download_manager_delegate_.reset(new ShellDownloadManagerDelegate());
    download_manager_delegate_->SetDownloadManager(
        BrowserContext::GetDownloadManager(this));
  }

  return download_manager_delegate_.get();
}

ResourceContext* ShellBrowserContext::GetResourceContext()  {
  return resource_context_.get();
}

BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

PushMessagingService* ShellBrowserContext::GetPushMessagingService() {
  return nullptr;
}

StorageNotificationService*
ShellBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

SSLHostStateDelegate* ShellBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

PermissionControllerDelegate*
ShellBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_.reset(new ShellPermissionManager());
  return permission_manager_.get();
}

ClientHintsControllerDelegate*
ShellBrowserContext::GetClientHintsControllerDelegate() {
  return client_hints_controller_delegate_;
}

BackgroundFetchDelegate* ShellBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* ShellBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_)
    background_sync_controller_.reset(new InertBackgroundSyncController());
  return background_sync_controller_.get();
}

BrowsingDataRemoverDelegate*
ShellBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

ContentIndexProvider* ShellBrowserContext::GetContentIndexProvider() {
  if (!content_index_provider_)
    content_index_provider_ = std::make_unique<ShellContentIndexProvider>();
  return content_index_provider_.get();
}

}  // namespace content
