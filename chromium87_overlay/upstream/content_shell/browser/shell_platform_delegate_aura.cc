// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_platform_data_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

#if defined(OS_HAIKU)
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/threading/thread_task_runner_handle.h"
#include "haiku_port/ozone/haiku_browser_chrome.h"
#include "url/gurl.h"
#endif

namespace content {

#if defined(OS_HAIKU)
namespace {

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
        ui_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  // Called on the UI thread when the Shell goes away.
  void Detach() { shell_ = nullptr; }

  // RCH_NAV_AFTER="8:https://a.example,20:https://b.example" navigates on a
  // timer through exactly the path the address bar uses.
  //
  // Re-navigation does not render: driving Page.navigate over DevTools moves
  // the target's URL but leaves the title at about:blank and produces no
  // frames, so the browser renders only the URL it was launched with. Whether
  // Shell::LoadURL is equally broken or whether this is a DevTools-only
  // problem cannot be answered over ssh, because typing into the address bar
  // needs a person at the machine. This hook answers it without one, and
  // stays as the way to reproduce a navigation bug without a human in the
  // loop.
  void ScheduleTestNavigations() {
    const char* spec = getenv("RCH_NAV_AFTER");
    if (spec == nullptr)
      return;
    for (const std::string& entry : base::SplitString(
             spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      const size_t colon = entry.find(':');
      if (colon == std::string::npos)
        continue;
      int seconds = 0;
      if (!base::StringToInt(entry.substr(0, colon), &seconds))
        continue;
      const GURL url(entry.substr(colon + 1));
      if (!url.is_valid())
        continue;
      fprintf(stderr, "[RCH] scheduling navigation to %s at t+%ds\n",
              url.spec().c_str(), seconds);
      ui_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&HaikuChromeClient::RunNavigate,
                         base::Unretained(this), url),
          base::TimeDelta::FromSeconds(seconds));
    }
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

 private:
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
    fprintf(stderr, "[RCH] LoadURL %s (shell=%p)\n", url.spec().c_str(),
            static_cast<void*>(shell_));
    if (shell_ != nullptr)
      shell_->LoadURL(url);
  }

  Shell* shell_;
  bool loading_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace
#endif  // defined(OS_HAIKU)

struct ShellPlatformDelegate::ShellData {
  gfx::NativeWindow window;
#if defined(OS_HAIKU)
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
  platform_->aura =
      std::make_unique<ShellPlatformDataAura>(default_window_size);
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  platform_->aura->ResizeWindow(initial_size);

  // Put the host on screen. ShellPlatformDataAura::ShowWindow() exists for
  // exactly this and nothing upstream calls it from here, because this
  // delegate is upstream's web-test configuration: the host backs an offscreen
  // compositor and is never meant to be visible. Every configuration that does
  // show a window goes through shell_platform_delegate_views.cc instead.
  //
  // Haiku builds with toolkit_views = false, so this delegate is what a real
  // browser window gets, and without this call the BWindow that ozone creates
  // stays hidden for its entire life. Confirmed by logging in
  // haiku_port/ozone/haiku_window.cc: the HaikuWindow constructor ran and
  // reported its 800x600 bounds, and HaikuWindow::Show() was never reached.
#if defined(OS_HAIKU)
  // Native chrome goes on before the window is shown, so the first frame is
  // already the right size and the content does not jump. AttachBrowserChrome
  // also calls HaikuWindow::SetTopInset(), which grows the BWindow by the
  // toolbar height so the web content keeps the size Chromium asked for.
  //
  // RCH_NO_TOOLBAR=1 skips all of it. The toolbar and the compositor share one
  // BWindow and one resize path, so when painting stops the first question is
  // which half broke it -- and answering that with an env switch costs a run
  // rather than a rebuild and a link, which on this machine is half an hour.
  shell_data.widget = platform_->aura->host()->GetAcceleratedWidget();
  if (getenv("RCH_NO_TOOLBAR") == nullptr) {
    shell_data.chrome = new HaikuChromeClient(shell);
    shell_data.chrome->ScheduleTestNavigations();
    const int inset =
        ui::AttachBrowserChrome(shell_data.widget, shell_data.chrome);
    fprintf(stderr, "[RCH] AttachBrowserChrome widget=%lu inset=%d\n",
            static_cast<unsigned long>(shell_data.widget), inset);
  } else {
    fprintf(stderr, "[RCH] toolbar suppressed by RCH_NO_TOOLBAR\n");
  }
#endif

  platform_->aura->ShowWindow();

  shell_data.window = platform_->aura->host()->window();
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];
  return shell_data.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
#if defined(OS_HAIKU)
  // The toolbar's BView outlives this, so the client object is left in place
  // and only stops pointing at the Shell. See the comment on the class.
  ShellData& shell_data = shell_data_map_[shell];
  if (shell_data.chrome != nullptr)
    shell_data.chrome->Detach();
#endif
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  aura::Window* content = shell->web_contents()->GetNativeView();
  aura::Window* parent = platform_->aura->host()->window();
  if (!parent->Contains(content))
    parent->AddChild(content);

  content->Show();

#if defined(OS_HAIKU)
  // Tell the WebContents it is visible.
  //
  // Nothing else does. Upstream's aura delegate is the web-test
  // configuration, where there is no visible window to report; every
  // configuration with a real window goes through
  // shell_platform_delegate_views instead, which Haiku does not build
  // (toolkit_views is off).
  //
  // Without this the page never renders: the network fetches it and
  // Shell::LoadURL runs, but Blink resolves no styles at all -- 2864
  // computed_style messages on a run that rendered against exactly 0 on a run
  // that did not, with one PresentCanvas for the life of the process.
  //
  // Calling RenderWidgetHostView::Show() instead does *not* work -- the view
  // is already showing every time it is checked here -- and adding a second
  // WasShown() from RenderViewReady() broke rendering outright: five
  // consecutive launches then rendered nothing. One call, from here.
  fprintf(stderr, "[RCH] SetContents: visibility before=%d\n",
          static_cast<int>(shell->web_contents()->GetVisibility()));
  shell->web_contents()->WasShown();
#endif
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->GetRenderWidgetHostView()->SetSize(content_size);
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
#if defined(OS_HAIKU)
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
#if defined(OS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ui::SetBrowserChromeAddress(it->second.widget, url.spec());
#endif
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {
#if defined(OS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ShellData& shell_data = it->second;
  shell_data.is_loading = loading;
  if (shell_data.chrome != nullptr)
    shell_data.chrome->SetLoading(loading);
  ui::SetBrowserChromeNavState(shell_data.widget, shell_data.can_go_back,
                               shell_data.can_go_forward, loading);
#endif
}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const base::string16& title) {
#if defined(OS_HAIKU)
  auto it = shell_data_map_.find(shell);
  if (it == shell_data_map_.end())
    return;
  ui::SetBrowserChromeTitle(it->second.widget, base::UTF16ToUTF8(title));
#endif
}

void ShellPlatformDelegate::RenderViewReady(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  return false;  // Shell destroys itself.
}

}  // namespace content
