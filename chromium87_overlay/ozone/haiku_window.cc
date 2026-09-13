#include "haiku_window.h"

#include <AppDefs.h>
#include <Cursor.h>
#include <Autolock.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "haiku_event_translation.h"
#include "haiku_window_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
namespace {

BRect ToBRect(const gfx::Rect& bounds) {
  const int width = std::max(1, bounds.width());
  const int height = std::max(1, bounds.height());
  return BRect(bounds.x(), bounds.y(), bounds.x() + width - 1,
               bounds.y() + height - 1);
}

}  // namespace

BRect HaikuWindow::FrameForContent(const gfx::Rect& content) const {
  BRect frame = ToBRect(content);
  // Grow downwards only: the content keeps the size Chromium asked for and the
  // extra height goes to the chrome above it.
  frame.bottom += top_inset_;
  return frame;
}

HaikuWindow::HaikuWindow(PlatformWindowDelegate* delegate,
                         HaikuWindowManager* manager,
                         const gfx::Rect& bounds)
    : StubWindow(delegate, false, bounds), manager_(manager), bounds_(bounds) {
  window_ = CreateBrowserNativeWindow(FrameForContent(bounds_));
  view_ = new HaikuContentView();

  // Wired up on the UI thread, before the window is shown, so the looper never
  // observes a half-initialised sink.
  view_->SetSinks(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&HaikuWindow::OnEventFromLooper,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&HaikuWindow::OnBoundsFromLooper,
                          weak_factory_.GetWeakPtr()));

  fprintf(stderr, "[RCH] HaikuWindow ctor bounds=%dx%d\n",
          bounds_.width(), bounds_.height());
  window_->AddChild(view_);
  widget_ = manager_->AddWindow(this, view_);
  manager_->SetWindowBounds(widget_, bounds_);
  fprintf(stderr, "[RCH] publishing widget=%lu\n", (unsigned long)widget_);
  delegate->OnAcceleratedWidgetAvailable(widget_);
}

HaikuWindow::~HaikuWindow() {
  fprintf(stderr, "[RCH] HaikuWindow DESTRUCTOR\n");
  delegate()->OnWillDestroyAcceleratedWidget();
  manager_->RemoveWindow(widget_, this);
  if (window_ != nullptr && window_->Lock())
    window_->Quit();
  window_ = nullptr;
  view_ = nullptr;
}

void HaikuWindow::OnEventFromLooper(std::unique_ptr<Event> event) {
  if (event->IsLocatedEvent()) {
    manager_->SetCursorScreenPoint(
        gfx::ToFlooredPoint(event->AsLocatedEvent()->root_location_f()));
  }
  delegate()->DispatchEvent(event.get());
}

void HaikuWindow::OnBoundsFromLooper(const gfx::Rect& bounds) {
  fprintf(stderr, "[RCH] OnBoundsFromLooper %dx%d\n", bounds.width(),
          bounds.height());
  if (bounds_ == bounds)
    return;
  bounds_ = bounds;
  manager_->SetWindowBounds(widget_, bounds_);
  delegate()->OnBoundsChanged(bounds_);
}

void HaikuWindow::Show(bool inactive) {
  fprintf(stderr, "[RCH] HaikuWindow::Show inactive=%d window=%p visible=%d\n",
          (int)inactive, (void*)window_, (int)visible_);
  if (window_ == nullptr || visible_)
    return;
  window_->Show();

  // Chromium asks for a frame at the screen origin, and on Haiku a BWindow's
  // frame is its *content* area: the title tab is drawn above it. A frame at
  // y = 0 therefore puts the tab off the top of the screen, where it cannot be
  // grabbed, moved or closed -- which is exactly how the first windows came up.
  // MoveOnScreen() knows the decorator's real size, so let it place the window
  // rather than guessing a tab height here. Done once, on the first show, so it
  // does not fight later SetBounds() calls.
  if (window_->Lock()) {
    window_->MoveOnScreen(B_MOVE_IF_PARTIALLY_OFFSCREEN);
    window_->Unlock();
  }

  visible_ = true;
  if (!inactive)
    window_->Activate(true);
}

void HaikuWindow::Hide() {
  if (window_ != nullptr && window_->Lock()) {
    window_->Hide();
    window_->Unlock();
  }
  visible_ = false;
}

void HaikuWindow::Close() {
  fprintf(stderr, "[RCH] HaikuWindow::Close\n");
  Hide();
  delegate()->OnClosed();
}

bool HaikuWindow::IsVisible() const {
  return visible_;
}

void HaikuWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  manager_->SetWindowBounds(widget_, bounds_);
  if (window_ != nullptr && window_->Lock()) {
    const BRect frame = FrameForContent(bounds);
    window_->MoveTo(frame.LeftTop());
    window_->ResizeTo(frame.Width(), frame.Height());
    window_->Unlock();
  }
  delegate()->OnBoundsChanged(bounds_);
}

gfx::Rect HaikuWindow::GetBounds() {
  return bounds_;
}

void HaikuWindow::SetTopInset(int inset) {
  inset = std::max(0, inset);
  if (top_inset_ == inset)
    return;
  top_inset_ = inset;

  // bounds_ is unchanged -- the content area keeps its size -- so the window
  // has to absorb the difference. Everything below runs with the window locked,
  // which is also the state AttachedToWindow() reads top_inset_ in.
  if (window_ == nullptr || !window_->Lock())
    return;

  const BRect frame = FrameForContent(bounds_);
  fprintf(stderr, "[RCH] SetTopInset %d -> window %gx%g\n", top_inset_,
          frame.Width(), frame.Height());

  // The content view must not follow the window through this resize.
  //
  // With B_FOLLOW_ALL_SIDES it does, and the sequence "grow the window, then
  // shrink the view back" makes the view briefly 30 pixels taller than it
  // ever should be. BView reports that with FrameResized(), which this port
  // forwards to PlatformWindowDelegate::OnBoundsChanged() -- so aura saw
  // 800x600, then 800x630, then 800x600 again, all before the first frame.
  // The renderer was left waiting on visual properties for a surface size
  // that no longer existed and never ran layout at all: the window came up
  // with the toolbar drawn and the web content permanently blank, with
  // exactly one PresentCanvas in the log and not one computed_style message
  // after it.
  //
  // Pinning the view for the duration means its size never actually changes
  // -- 800x600 before and after -- so BView issues no FrameResized() and aura
  // sees no bounds change at all, which is the truth: the content area is the
  // same size, it has only moved down.
  if (view_ != nullptr)
    view_->SetResizingMode(B_FOLLOW_NONE);
  window_->ResizeTo(frame.Width(), frame.Height());
  if (view_ != nullptr) {
    view_->SetTopInset(top_inset_);
    view_->MoveTo(0, top_inset_);
    view_->ResizeTo(window_->Bounds().Width(),
                    window_->Bounds().Height() - top_inset_);
    view_->SetResizingMode(B_FOLLOW_ALL_SIDES);
  }
  window_->Unlock();
}

void HaikuWindow::SetTitle(const base::string16& title) {
  const std::string utf8 = base::UTF16ToUTF8(title);
  if (window_ != nullptr && window_->Lock()) {
    window_->SetTitle(utf8.c_str());
    window_->Unlock();
  }
}

void HaikuWindow::Activate() {
  if (window_ != nullptr)
    window_->Activate(true);
}

void HaikuWindow::Minimize() {
  if (window_ != nullptr)
    window_->Minimize(true);
}

void HaikuWindow::Restore() {
  if (window_ != nullptr)
    window_->Minimize(false);
}

void HaikuWindow::SetCursor(PlatformCursor cursor) {
  // HaikuCursorFactory hands out BCursor*, so this is the whole of it: no
  // bitmap is drawn by Chromium and app_server shows its own cursor.
  if (view_ == nullptr || window_ == nullptr || !window_->Lock())
    return;
  view_->SetViewCursor(static_cast<const BCursor*>(cursor), true);
  window_->Unlock();
}

PlatformWindowState HaikuWindow::GetPlatformWindowState() const {
  return PlatformWindowState::kNormal;
}

}  // namespace ui
