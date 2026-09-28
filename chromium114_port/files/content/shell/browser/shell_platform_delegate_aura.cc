// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_platform_data_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

#if BUILDFLAG(IS_HAIKU)
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
// Layer violation, and a deliberate one. content/shell's DEPS allows
// +content/public only, because the shell is meant to be the canonical
// embedder. 87 did not need the exception: WebContents::GetManifest() was
// public then. It was removed, and the only way to the parsed manifest now is
// ManifestManagerHost, which lives in content/browser. An embedder outside
// the content tree could not do this; content_shell is inside it and already
// links the browser library, so the cost is the rule and not the build.
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/public/browser/page.h"
#include "haiku_port/ozone/haiku_browser_chrome.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#endif

namespace content {

#if BUILDFLAG(IS_HAIKU)
namespace {

// Defined below, next to the rest of the install code. Declared here because
// the toolbar client is written before it and calls into it.
void OnManifestForInstall(WebContents* web_contents,
                          const blink::mojom::Manifest& manifest);

// Drives the native Haiku toolbar. Its methods run on the BWindow's looper
// thread, so each one hops to the UI thread before touching the Shell.
//
// Deliberately never destroyed. The BView that holds this pointer lives as
// long as the BWindow, which ozone tears down on its own schedule, and a
// dangling client would be a use-after-free on the looper thread -- the one
// thread whose crashes are hardest to attribute here. `shell_` is cleared in
// CleanUp() instead, so what leaks is one small object per window in a
// process that has one window.
class HaikuChromeClient : public ui::HaikuBrowserChromeClient {
 public:
  explicit HaikuChromeClient(Shell* shell)
      : shell_(shell),
        ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  // Called on the UI thread when the Shell goes away.
  void Detach() { shell_ = nullptr; }

  // RCH_INSTALL_AFTER=<seconds> presses the install button on a timer.
  //
  // The machine this port is developed on is a physical laptop reached over
  // ssh: there is no way to click anything from here, and a feature that can
  // only be tested by a person standing at it will not be tested. This runs
  // the same path the button does -- OnInstall() on the looper thread is the
  // only step it skips, and that step is a PostTask.
  void ScheduleTestInstall() {
    const char* spec = getenv("RCH_INSTALL_AFTER");
    if (spec == nullptr)
      return;
    int seconds = 0;
    if (!base::StringToInt(spec, &seconds))
      return;
    fprintf(stderr, "[RCH] scheduling install at t+%ds\n", seconds);
    fflush(stderr);
    ui_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HaikuChromeClient::RunInstall,
                       base::Unretained(this)),
        base::Seconds(seconds));
  }

  // Which of Reload and Stop the one button means right now. Written on the
  // UI thread from SetIsLoading(), read on the looper thread; a stale read
  // costs at most one reload of a page that was still loading.
  void SetLoading(bool loading) { loading_ = loading; }

  void OnBack() override { Post(Action::kBack); }
  void OnForward() override { Post(Action::kForward); }
  void OnReloadOrStop() override {
    Post(loading_ ? Action::kStop : Action::kReload);
  }

  void OnNavigate(const std::string& text) override {
    const GURL url = Fixup(text);
    if (!url.is_valid())
      return;
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HaikuChromeClient::RunNavigate,
                                  base::Unretained(this), url));
  }

  void OnInstall() override {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HaikuChromeClient::RunInstall,
                       base::Unretained(this)));
  }

 private:
  // On the UI thread: ask for the manifest again and install what comes back.
  void RunInstall() {
    if (shell_ == nullptr || shell_->web_contents() == nullptr)
      return;
    WebContents* contents = shell_->web_contents();
    ManifestManagerHost::GetOrCreateForPage(contents->GetPrimaryPage())
        ->GetManifest(base::BindOnce(
            [](WebContents* contents, const GURL&,
               blink::mojom::ManifestPtr manifest) {
              if (manifest)
                OnManifestForInstall(contents, *manifest);
            },
            contents));
  }

  enum class Action { kBack, kForward, kReload, kStop };

  ~HaikuChromeClient() = default;

  // "google.com" is what a person types into an address bar, and GURL on its
  // own rejects it for having no scheme. Default the scheme to https:// --
  // that is what every modern browser assumes, and defaulting to http:// only
  // forced an extra http->https redirect round trip on sites (naver, google)
  // that immediately upgrade, which on this 2-core machine is a visible delay.
  static GURL Fixup(const std::string& text) {
    std::string trimmed;
    base::TrimWhitespaceASCII(text, base::TRIM_ALL, &trimmed);
    if (trimmed.empty())
      return GURL();
    const GURL as_typed(trimmed);
    if (as_typed.is_valid() && as_typed.has_scheme())
      return as_typed;
    return GURL("https://" + trimmed);
  }

  void Post(Action action) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HaikuChromeClient::RunAction,
                                  base::Unretained(this), action));
  }

  void RunAction(Action action) {
    if (shell_ == nullptr)
      return;
    switch (action) {
      case Action::kBack:
        shell_->GoBackOrForward(-1);
        return;
      case Action::kForward:
        shell_->GoBackOrForward(1);
        return;
      case Action::kReload:
        shell_->Reload();
        return;
      case Action::kStop:
        shell_->Stop();
        return;
    }
  }

  void RunNavigate(const GURL& url) {
    if (shell_ != nullptr)
      shell_->LoadURL(url);
  }

  raw_ptr<Shell> shell_;
  bool loading_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

// Closes the Shell when its BWindow's close button is pressed.
//
// The path is BrowserNativeWindow::QuitRequested() -> HaikuContentView ->
// HaikuWindow::OnCloseFromLooper() -> PlatformWindowDelegate::OnCloseRequest()
// -> WindowTreeHost::OnHostCloseRequested() -> here. Before this existed the
// BWindow simply hid itself and the Shell lived on, invisible, until the
// process was killed; with one window per Shell that would also leak a whole
// WebContents per closed window.
class ShellHostCloseObserver : public aura::WindowTreeHostObserver {
 public:
  ShellHostCloseObserver(Shell* shell, aura::WindowTreeHost* host)
      : shell_(shell), host_(host) {
    host_->AddObserver(this);
  }
  ~ShellHostCloseObserver() override { host_->RemoveObserver(this); }

  ShellHostCloseObserver(const ShellHostCloseObserver&) = delete;
  ShellHostCloseObserver& operator=(const ShellHostCloseObserver&) = delete;

  void OnHostCloseRequested(aura::WindowTreeHost* host) override {
    // Not synchronously: Shell::Close() deletes the Shell, and with it this
    // observer and the host that is iterating its observer list right now.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShellHostCloseObserver::CloseShell, shell_));
  }

 private:
  static void CloseShell(Shell* shell) {
    // The Shell may already be gone (a second click on the close button
    // before the first task ran), so check it is still registered.
    if (base::Contains(Shell::windows(), shell))
      shell->Close();
  }

  raw_ptr<Shell> shell_;
  raw_ptr<aura::WindowTreeHost> host_;
};

}  // namespace
#endif  // BUILDFLAG(IS_HAIKU)

struct ShellPlatformDelegate::ShellData {
  gfx::NativeWindow window;
#if BUILDFLAG(IS_HAIKU)
  // This Shell's own WindowTreeHost and BWindow. Declared before
  // close_observer so the observer unregisters from the host before the host
  // is destroyed.
  std::unique_ptr<ShellPlatformDataAura> aura;
  std::unique_ptr<ShellHostCloseObserver> close_observer;
  HaikuChromeClient* chrome = nullptr;
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  bool can_go_back = false;
  bool can_go_forward = false;
  bool is_loading = false;
#endif
};

struct ShellPlatformDelegate::PlatformData {
  std::unique_ptr<ShellPlatformDataAura> aura;
};

ShellPlatformDelegate::ShellPlatformDelegate() = default;
ShellPlatformDelegate::~ShellPlatformDelegate() = default;

ShellPlatformDataAura* ShellPlatformDelegate::GetShellPlatformDataAura() {
  return platform_->aura.get();
}

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  platform_ = std::make_unique<PlatformData>();
  // No display::Screen here. 87's ShellPlatformDataAura did not make one and
  // this delegate had to; 114's does, through aura::ScopedScreenOzone, which
  // installs one only if none exists and calls the Initialize() that a bare
  // ScreenOzone needs. Constructing one here instead SEGVs at 0x0 before the
  // first window, which is how this comment came to be written.
  platform_->aura =
      std::make_unique<ShellPlatformDataAura>(default_window_size);
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

#if BUILDFLAG(IS_HAIKU)
  // One host -- one BWindow -- per Shell.
  //
  // Upstream shares the single host Initialize() created among every Shell,
  // because this delegate is the web-test configuration: the host is an
  // offscreen surface and a second Shell is a popup no one looks at. Here the
  // host is the browser window, and sharing it put a link's new window inside
  // the old one.
  //
  // The first Shell adopts the host Initialize() made rather than discarding
  // it; every later Shell gets a fresh one at its requested size.
  if (platform_->aura)
    shell_data.aura = std::move(platform_->aura);
  else
    shell_data.aura = std::make_unique<ShellPlatformDataAura>(initial_size);
  ShellPlatformDataAura* aura = shell_data.aura.get();
  shell_data.close_observer =
      std::make_unique<ShellHostCloseObserver>(shell, aura->host());
#else
  ShellPlatformDataAura* aura = platform_->aura.get();
#endif

  aura->ResizeWindow(initial_size);

#if BUILDFLAG(IS_HAIKU)
  // Native chrome goes on before the window is shown, so the first frame is
  // already the right size and the content does not jump. AttachBrowserChrome
  // also calls HaikuWindow::SetTopInset(), which grows the BWindow by the
  // toolbar height so the web content keeps the size Chromium asked for.
  //
  // RCH_NO_TOOLBAR=1 skips all of it, which is what an installed web app
  // sets: an app is a window onto one site and has no address to type.
  shell_data.widget = aura->host()->GetAcceleratedWidget();
  if (getenv("RCH_NO_TOOLBAR") == nullptr) {
    shell_data.chrome = new HaikuChromeClient(shell);
    shell_data.chrome->ScheduleTestInstall();
    const int inset =
        ui::AttachBrowserChrome(shell_data.widget, shell_data.chrome);
    fprintf(stderr, "[RCH] AttachBrowserChrome widget=%lu inset=%d\n",
            static_cast<unsigned long>(shell_data.widget), inset);
  } else {
    fprintf(stderr, "[RCH] toolbar suppressed by RCH_NO_TOOLBAR\n");
  }
#endif

  aura->ShowWindow();

  shell_data.window = aura->host()->window();
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];
  return shell_data.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
#if BUILDFLAG(IS_HAIKU)
  // The toolbar's BView outlives this, so the client object is left in place
  // and only stops pointing at the Shell. See the comment on the class.
  ShellData& shell_data = shell_data_map_[shell];
  if (shell_data.chrome != nullptr)
    shell_data.chrome->Detach();

  // Take the WebContents' window out of this Shell's root before the root --
  // and the BWindow behind it -- is destroyed with shell_data below.
  aura::Window* content = shell->web_contents()->GetNativeView();
  if (content != nullptr && content->parent() != nullptr)
    content->parent()->RemoveChild(content);
#endif
  shell_data_map_.erase(shell);
}

#if BUILDFLAG(IS_HAIKU)
namespace {

void FocusContentsSoon(Shell* shell, int attempts_left);

bool ShellIsAlive(Shell* shell) {
  return base::Contains(Shell::windows(), shell);
}

// Focus the page, retrying on later turns of the loop while there is still
// nothing focusable. RenderWidgetHostViewAura::Focus() is a no-op until the
// view has a focus client and its window can take focus, and the view itself
// is created and shown asynchronously, so the first attempt -- made from
// SetContents -- routinely comes too early.
void FocusContentsNow(Shell* shell, int attempts_left) {
  if (!ShellIsAlive(shell))
    return;
  WebContents* web_contents = shell->web_contents();
  if (web_contents == nullptr)
    return;
  RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (view != nullptr && view->HasFocus())
    return;
  web_contents->Focus();
  view = web_contents->GetRenderWidgetHostView();
  if (view != nullptr && view->HasFocus())
    return;
  if (attempts_left <= 0) {
    fprintf(stderr, "[RCH] web contents never took focus; view=%p\n",
            static_cast<void*>(view));
    return;
  }
  FocusContentsSoon(shell, attempts_left - 1);
}

void FocusContentsSoon(Shell* shell, int attempts_left) {
  // Spread the retries over real time: what is being waited for is the view's
  // creation and the host window's Show(), and posting without a delay would
  // burn every attempt in the same millisecond.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&FocusContentsNow, shell, attempts_left),
      base::Milliseconds(50));
}

}  // namespace
#endif  // BUILDFLAG(IS_HAIKU)

void ShellPlatformDelegate::SetContents(Shell* shell) {
  aura::Window* content = shell->web_contents()->GetNativeView();
#if BUILDFLAG(IS_HAIKU)
  aura::Window* parent = shell_data_map_[shell].aura->host()->window();
#else
  aura::Window* parent = platform_->aura->host()->window();
#endif
  if (!parent->Contains(content))
    parent->AddChild(content);

  content->Show();

#if BUILDFLAG(IS_HAIKU)
  // Tell the WebContents it is visible. Nothing else does: upstream's aura
  // delegate is the web-test configuration, where there is no visible window
  // to report, and every configuration with a real window goes through
  // shell_platform_delegate_views instead, which Haiku does not build.
  // Without this the page never renders.
  shell->web_contents()->WasShown();

  // Nothing focuses the contents in this configuration either, and until the
  // page was clicked document.hasFocus() was false and every key was dropped.
  FocusContentsSoon(shell, 30);
#endif
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->GetRenderWidgetHostView()->SetSize(content_size);
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
#if BUILDFLAG(IS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ShellData& shell_data = it->second;
  switch (control) {
    case BACK_BUTTON:
      shell_data.can_go_back = is_enabled;
      break;
    case FORWARD_BUTTON:
      shell_data.can_go_forward = is_enabled;
      break;
    case STOP_BUTTON:
      // The toolbar has one button for reload and stop, and SetIsLoading()
      // already says which it should be.
      return;
  }
  ui::SetBrowserChromeNavState(shell_data.widget, shell_data.can_go_back,
                               shell_data.can_go_forward,
                               shell_data.is_loading);
#endif
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
#if BUILDFLAG(IS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ui::SetBrowserChromeAddress(it->second.widget, url.spec());
#endif
}

#if BUILDFLAG(IS_HAIKU)
namespace {

// Is this page something we could install as a Haiku application?
//
// Chrome answers this in chrome/browser/web_applications, which content_shell
// does not have and this port will not be growing. But everything the answer
// needs is in the content layer already: the manifest, and
// WebContents::DownloadImage() for its icons.
//
// The test below is Chrome's installability test minus the service worker.
// Chrome requires a fetch handler because an installed app there is expected
// to work offline; here an installed app is a Deskbar entry that launches
// content_shell at a URL, which is useful whether or not the site has a
// worker. Requiring one would rule out most of what a person would actually
// want to install on this machine.
bool IsInstallable(const blink::mojom::Manifest& manifest,
                   std::string* why_not) {
  if (blink::IsEmptyManifest(manifest)) {
    *why_not = "no manifest";
    return false;
  }
  if (!manifest.start_url.is_valid()) {
    *why_not = "no valid start_url";
    return false;
  }
  if (!manifest.name.has_value() && !manifest.short_name.has_value()) {
    *why_not = "no name or short_name";
    return false;
  }
  switch (manifest.display) {
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kMinimalUi:
      break;
    default:
      *why_not = "display is not standalone/fullscreen/minimal-ui";
      return false;
  }
  for (const auto& icon : manifest.icons) {
    if (!icon.src.is_valid())
      continue;
    for (const auto& purpose : icon.purpose) {
      if (purpose == blink::mojom::ManifestImageResource_Purpose::ANY ||
          purpose == blink::mojom::ManifestImageResource_Purpose::MASKABLE) {
        return true;
      }
    }
  }
  *why_not = "no usable icon";
  return false;
}

std::string AppNameOf(const blink::mojom::Manifest& manifest) {
  if (manifest.name.has_value())
    return base::UTF16ToUTF8(manifest.name.value());
  if (manifest.short_name.has_value())
    return base::UTF16ToUTF8(manifest.short_name.value());
  return std::string();
}

// A name that can be a directory and a Deskbar entry: no slashes, no leading
// dot, not empty, and short enough to read in a menu.
std::string SanitizeAppName(const std::string& name, const GURL& start_url) {
  std::string out;
  for (char c : name) {
    if (c == '/' || c == '\\' || c == ':' || c == '\n' || c == '\r' ||
        c == '\t') {
      out += ' ';
    } else if (static_cast<unsigned char>(c) < 0x20) {
      continue;
    } else {
      out += c;
    }
  }
  base::TrimWhitespaceASCII(out, base::TRIM_ALL, &out);
  while (!out.empty() && out[0] == '.')
    out.erase(0, 1);
  if (out.size() > 48)
    out.resize(48);
  base::TrimWhitespaceASCII(out, base::TRIM_ALL, &out);
  if (out.empty())
    out = start_url.host();
  if (out.empty())
    out = "Web App";
  return out;
}

// Where an installed app lives. ~/config/non-packaged/apps is the standard
// place for a user's own applications on Haiku.
base::FilePath AppDirFor(const std::string& app_name) {
  const char* home = getenv("HOME");
  if (home == nullptr)
    home = "/boot/home";
  return base::FilePath(home)
      .Append("config")
      .Append("non-packaged")
      .Append("apps")
      .Append(app_name);
}

// The launcher: a shell script that starts this very content_shell at the
// manifest's start_url with the toolbar off.
//
// A script rather than a copied binary. content_shell is 275 MB and an
// installed app is not a second browser; it is the same browser pointed at
// one URL. Tracker runs an executable script on double-click and Deskbar
// lists it, so a script is a first-class application here.
bool WriteLauncher(const base::FilePath& dir,
                   const std::string& app_name,
                   const GURL& start_url,
                   std::string* error) {
  base::FilePath shell_path;
  if (!base::PathService::Get(base::FILE_EXE, &shell_path)) {
    *error = "cannot find my own path";
    return false;
  }

  std::string script;
  script += "#!/bin/sh\n";
  script += "# " + app_name + "\n";
  script += "#\n";
  script += "# Installed by R Chromium from " + start_url.spec() + "\n";
  script += "# Delete this directory to uninstall.\n";
  script += "\n";
  script += "SHELL_BIN=\"" + shell_path.value() + "\"\n";
  script += "[ -x \"$SHELL_BIN\" ] || SHELL_BIN=/boot/system/apps/RChromium/content_shell\n";
  script += "APPDIR=$(dirname \"$SHELL_BIN\")\n";
  script += "\n";
  script += "# Blink aborts without a fontconfig file and Haiku ships no /etc/fonts.\n";
  script += "if [ -z \"$FONTCONFIG_FILE\" ]; then\n";
  script += "\tif [ -r \"$APPDIR/rchromium-fonts.conf\" ]; then\n";
  script += "\t\tFONTCONFIG_FILE=\"$APPDIR/rchromium-fonts.conf\"\n";
  script += "\telse\n";
  script += "\t\tFONTCONFIG_FILE=/boot/home/rchromium-fonts.conf\n";
  script += "\tfi\n";
  script += "\texport FONTCONFIG_FILE\n";
  script += "fi\n";
  script += "\n";
  script += "# No toolbar: an installed app is a window onto one site.\n";
  script += "RCH_NO_TOOLBAR=1\n";
  script += "export RCH_NO_TOOLBAR\n";
  script += "\n";
  script += "# The window keeps the app's name rather than following the page.\n";
  script += "RCH_APP_NAME=\"" + app_name + "\"\n";
  script += "export RCH_APP_NAME\n";
  script += "\n";
  script += "# 256 descriptors is a low ceiling for a browser: every shared\n";
  script += "# memory region here costs two, and a page that fetches a hundred\n";
  script += "# ES modules at once wants a few hundred.\n";
  script += "ulimit -n 8192 2>/dev/null\n";
  script += "\n";
  script += "exec \"$SHELL_BIN\" \\\n";
  script += "\t--ozone-platform=haiku \\\n";
  script += "\t--single-process \\\n";
  script += "\t--disable-gpu \\\n";
  script += "\t--in-process-gpu \\\n";
  script += "\t--disable-gpu-compositing \\\n";
  // --data-path, not --user-data-dir: that is Chrome's switch and
  // content_shell ignores it, which in 87 left every installed app sharing
  // the browser's profile.
  script += "\t--data-path=\"" + dir.value() + "/profile\" \\\n";
  script += "\t\"" + start_url.spec() + "\" \"$@\"\n";

  const base::FilePath launcher = dir.Append(app_name);
  if (!base::WriteFile(launcher, script)) {
    *error = "cannot write " + launcher.value();
    return false;
  }
  if (chmod(launcher.value().c_str(), 0755) != 0) {
    *error = "cannot chmod " + launcher.value();
    return false;
  }
  return true;
}

void OnManifest(gfx::AcceleratedWidget widget,
                const GURL& page_url,
                const GURL& manifest_url,
                blink::mojom::ManifestPtr manifest) {
  if (!manifest) {
    ui::SetBrowserChromeInstallable(widget, false, std::string());
    return;
  }
  std::string why_not;
  const bool ok = IsInstallable(*manifest, &why_not);
  const std::string name = AppNameOf(*manifest);
  fprintf(stderr,
          "[RCH] manifest page=%s url=%s installable=%d name=\"%s\" "
          "start=%s display=%d icons=%zu%s%s\n",
          page_url.spec().c_str(), manifest_url.spec().c_str(), ok ? 1 : 0,
          name.c_str(), manifest->start_url.spec().c_str(),
          static_cast<int>(manifest->display), manifest->icons.size(),
          ok ? "" : " why=", ok ? "" : why_not.c_str());
  fflush(stderr);
  ui::SetBrowserChromeInstallable(widget, ok, name);
}

// The icon to install with. Prefer the largest square one the manifest
// declares; Haiku wants 32x32 and 16x16 and downscaling a big icon beats
// upscaling a small one. A manifest may declare "any" as its size, which
// means a vector -- take those first, since they rasterise to whatever is
// asked for.
GURL BestIconUrl(const blink::mojom::Manifest& manifest) {
  GURL best;
  int best_area = -1;
  for (const auto& icon : manifest.icons) {
    if (!icon.src.is_valid())
      continue;
    bool usable = false;
    for (const auto& purpose : icon.purpose) {
      if (purpose == blink::mojom::ManifestImageResource_Purpose::ANY ||
          purpose == blink::mojom::ManifestImageResource_Purpose::MASKABLE) {
        usable = true;
        break;
      }
    }
    if (!usable)
      continue;
    if (icon.sizes.empty()) {
      // "any" -- a vector. Nothing beats it.
      return icon.src;
    }
    for (const auto& size : icon.sizes) {
      if (size.width() != size.height())
        continue;
      const int area = size.width() * size.height();
      if (area > best_area) {
        best_area = area;
        best = icon.src;
      }
    }
  }
  // Nothing square: take the first usable one rather than none.
  if (!best.is_valid()) {
    for (const auto& icon : manifest.icons) {
      if (icon.src.is_valid())
        return icon.src;
    }
  }
  return best;
}

// Hand the downloaded icon to the BeAPI side, which scales it and writes the
// file's icon attributes. SkColor is 0xAARRGGBB non-premultiplied, which is
// exactly what SetFileIcon() documents wanting, so neither side has to know
// the other's bitmap type.
void OnIconDownloaded(const base::FilePath& launcher,
                      int /*id*/,
                      int http_status_code,
                      const GURL& image_url,
                      const std::vector<SkBitmap>& bitmaps,
                      const std::vector<gfx::Size>& /*sizes*/) {
  if (bitmaps.empty()) {
    fprintf(stderr, "[RCH] icon download failed (%d) for %s\n",
            http_status_code, image_url.spec().c_str());
    fflush(stderr);
    return;
  }
  const SkBitmap* best = &bitmaps[0];
  for (const SkBitmap& bitmap : bitmaps) {
    if (bitmap.width() * bitmap.height() > best->width() * best->height())
      best = &bitmap;
  }
  const int w = best->width();
  const int h = best->height();
  if (w <= 0 || h <= 0)
    return;

  std::vector<uint32_t> argb(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x)
      argb[static_cast<size_t>(y) * w + x] = best->getColor(x, y);
  }

  const bool ok = ui::SetFileIcon(launcher.value(), argb.data(), w, h);
  fprintf(stderr, "[RCH] icon %dx%d -> %s: %s\n", w, h,
          launcher.value().c_str(), ok ? "set" : "FAILED");
  fflush(stderr);
}

// Put the app in Deskbar's Applications menu.
//
// Deskbar does not scan ~/config/non-packaged/apps. It lists what is in
// ~/config/settings/deskbar/menu/Applications, which is a directory of
// symlinks. Writing the launcher without this leaves an app that exists and
// cannot be found.
bool LinkIntoDeskbar(const base::FilePath& launcher,
                     const std::string& app_name,
                     std::string* error) {
  const char* home = getenv("HOME");
  if (home == nullptr)
    home = "/boot/home";
  const base::FilePath menu = base::FilePath(home)
                                  .Append("config")
                                  .Append("settings")
                                  .Append("deskbar")
                                  .Append("menu")
                                  .Append("Applications");
  base::File::Error mkdir_error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(menu, &mkdir_error)) {
    *error = "cannot create " + menu.value();
    return false;
  }
  const base::FilePath link = menu.Append(app_name);
  // Replace an older link to the same app rather than failing on it.
  unlink(link.value().c_str());
  if (symlink(launcher.value().c_str(), link.value().c_str()) != 0) {
    *error = "cannot link " + link.value();
    return false;
  }
  return true;
}

// Second half of the install: the manifest is asked for again at the moment
// the button is pressed rather than cached, because the page may have
// navigated since the button appeared and installing the previous site's app
// would be a surprise.
void OnManifestForInstall(WebContents* web_contents,
                          const blink::mojom::Manifest& manifest) {
  std::string why_not;
  if (!IsInstallable(manifest, &why_not)) {
    fprintf(stderr, "[RCH] install refused: %s\n", why_not.c_str());
    fflush(stderr);
    return;
  }
  const std::string app_name =
      SanitizeAppName(AppNameOf(manifest), manifest.start_url);
  const base::FilePath dir = AppDirFor(app_name);

  base::File::Error mkdir_error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(dir, &mkdir_error)) {
    fprintf(stderr, "[RCH] install failed: cannot create %s (%d)\n",
            dir.value().c_str(), static_cast<int>(mkdir_error));
    fflush(stderr);
    return;
  }

  std::string error;
  if (!WriteLauncher(dir, app_name, manifest.start_url, &error)) {
    fprintf(stderr, "[RCH] install failed: %s\n", error.c_str());
    fflush(stderr);
    return;
  }

  const base::FilePath launcher = dir.Append(app_name);

  std::string link_error;
  const bool linked = LinkIntoDeskbar(launcher, app_name, &link_error);
  fprintf(stderr, "[RCH] installed \"%s\" -> %s (deskbar: %s)\n",
          app_name.c_str(), launcher.value().c_str(),
          linked ? "listed" : link_error.c_str());
  fflush(stderr);

  // The icon comes after: the app is already usable without one, and a
  // download that fails should not fail the install.
  const GURL icon_url = BestIconUrl(manifest);
  if (icon_url.is_valid() && web_contents != nullptr) {
    web_contents->DownloadImage(
        icon_url, /*is_favicon=*/false, /*preferred_size=*/gfx::Size(128, 128),
        /*max_bitmap_size=*/512, /*bypass_cache=*/false,
        base::BindOnce(&OnIconDownloaded, launcher));
  }
}

}  // namespace
#endif  // BUILDFLAG(IS_HAIKU)

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {
#if BUILDFLAG(IS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ShellData& shell_data = it->second;
  shell_data.is_loading = loading;
  if (shell_data.chrome != nullptr)
    shell_data.chrome->SetLoading(loading);
  ui::SetBrowserChromeNavState(shell_data.widget, shell_data.can_go_back,
                               shell_data.can_go_forward, loading);

  // Ask once per load, when the load finishes. The manifest comes from the
  // renderer and answers on the UI thread; asking earlier gets the manifest
  // of whatever was there before.
  if (!loading && shell->web_contents() != nullptr) {
    WebContents* contents = shell->web_contents();
    const GURL page_url = contents->GetLastCommittedURL();
    ManifestManagerHost::GetOrCreateForPage(contents->GetPrimaryPage())
        ->GetManifest(
            base::BindOnce(&OnManifest, shell_data.widget, page_url));
  }
#endif
}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {
#if BUILDFLAG(IS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ui::SetBrowserChromeTitle(it->second.widget, base::UTF16ToUTF8(title));

  // And the window itself, which is what the window tab and Deskbar show.
  //
  // An installed web app is launched with RCH_APP_NAME and keeps that name
  // whatever the page calls itself: it is one application, and a window that
  // renames itself as the user moves around inside it does not look like one.
  // A browser window follows the page.
  const char* app_name = getenv("RCH_APP_NAME");
  ui::SetNativeWindowTitle(it->second.widget,
                           app_name != nullptr && app_name[0] != '\0'
                               ? std::string(app_name)
                               : base::UTF16ToUTF8(title));
#endif
}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  return false;  // Shell destroys itself.
}

}  // namespace content
