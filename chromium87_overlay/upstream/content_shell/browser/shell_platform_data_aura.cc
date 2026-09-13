// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_data_aura.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/default_activation_client.h"

#if defined(OS_FUCHSIA)
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"
#endif

#if defined(USE_OZONE)
#include "ui/aura/screen_ozone.h"
#include "ui/base/ui_base_features.h"
#endif

namespace content {

namespace {

// The pinned qtwebengine-chromium distribution ships only the Ozone files of
// ui/aura/test, so aura::test::TestFocusClient and TestWindowParentingClient
// are unavailable and //ui/aura:test_support cannot be built. Both classes are
// small policy objects; equivalent local versions keep content_shell free of
// any test-only target.
class ShellFocusClient : public aura::client::FocusClient,
                         public aura::WindowObserver {
 public:
  explicit ShellFocusClient(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetFocusClient(root_window, this);
  }
  ~ShellFocusClient() override {
    aura::client::SetFocusClient(root_window_, nullptr);
  }

  ShellFocusClient(const ShellFocusClient&) = delete;
  ShellFocusClient& operator=(const ShellFocusClient&) = delete;

  void AddObserver(aura::client::FocusChangeObserver* observer) override {
    focus_observers_.AddObserver(observer);
  }
  void RemoveObserver(aura::client::FocusChangeObserver* observer) override {
    focus_observers_.RemoveObserver(observer);
  }
  void FocusWindow(aura::Window* window) override {
    if (window == focused_window_)
      return;
    if (focused_window_ && observer_manager_.IsObserving(focused_window_))
      observer_manager_.Remove(focused_window_);
    aura::Window* old_focused_window = focused_window_;
    focused_window_ = window;
    if (focused_window_)
      observer_manager_.Add(focused_window_);
    for (auto& observer : focus_observers_)
      observer.OnWindowFocused(focused_window_, old_focused_window);
    aura::client::FocusChangeObserver* observer =
        aura::client::GetFocusChangeObserver(old_focused_window);
    if (observer)
      observer->OnWindowFocused(focused_window_, old_focused_window);
    observer = aura::client::GetFocusChangeObserver(focused_window_);
    if (observer)
      observer->OnWindowFocused(focused_window_, old_focused_window);
  }
  void ResetFocusWithinActiveWindow(aura::Window* window) override {
    if (!window->Contains(focused_window_))
      return;
    FocusWindow(window);
  }
  aura::Window* GetFocusedWindow() override { return focused_window_; }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window, focused_window_);
    FocusWindow(nullptr);
  }

 private:
  aura::Window* root_window_;
  aura::Window* focused_window_ = nullptr;
  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_{this};
  base::ObserverList<aura::client::FocusChangeObserver>::Unchecked
      focus_observers_;
};

class ShellWindowParentingClient : public aura::client::WindowParentingClient {
 public:
  explicit ShellWindowParentingClient(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowParentingClient(root_window, this);
  }
  ~ShellWindowParentingClient() override {
    aura::client::SetWindowParentingClient(root_window_, nullptr);
  }

  ShellWindowParentingClient(const ShellWindowParentingClient&) = delete;
  ShellWindowParentingClient& operator=(const ShellWindowParentingClient&) =
      delete;

  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;
};

}  // namespace

namespace {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root)
      : root_(root), has_bounds_(!root->bounds().IsEmpty()) {}

  ~FillLayout() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override {
    // If window bounds were not set previously then resize all children to
    // match the size of the parent.
    if (!has_bounds_) {
      has_bounds_ = true;
      for (aura::Window* child : root_->children())
        SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
    }
  }

  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  void OnWindowRemovedFromLayout(aura::Window* child) override {}

  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;
  bool has_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  CHECK(aura::Env::GetInstance());

#if defined(USE_OZONE)
  // Setup global display::Screen singleton.
  if (features::IsUsingOzonePlatform() && !display::Screen::GetScreen())
    screen_ = std::make_unique<aura::ScreenOzone>();
#endif  // defined(USE_OZONE)

  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(initial_size);

#if defined(OS_FUCHSIA)
  // When using Scenic Ozone platform we need to supply a view_token to the
  // window. This is not necessary when using the headless ozone platform.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .needs_view_token) {
    ui::fuchsia::InitializeViewTokenAndPresentView(&properties);
  }
#endif

  host_ = aura::WindowTreeHost::Create(std::move(properties));
  host_->InitHost();
  host_->window()->Show();
  host_->window()->SetLayoutManager(new FillLayout(host_->window()));

  focus_client_ =
      std::make_unique<ShellFocusClient>(host_->window());

  new wm::DefaultActivationClient(host_->window());
  capture_client_ =
      std::make_unique<aura::client::DefaultCaptureClient>(host_->window());
  window_parenting_client_ =
      std::make_unique<ShellWindowParentingClient>(host_->window());
}

ShellPlatformDataAura::~ShellPlatformDataAura() = default;

void ShellPlatformDataAura::ShowWindow() {
  host_->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  host_->SetBoundsInPixels(gfx::Rect(size));
}

}  // namespace content
