#include "haiku_window_manager.h"

#include "base/check.h"

namespace ui {

HaikuWindowManager::HaikuWindowManager() = default;
HaikuWindowManager::~HaikuWindowManager() = default;

gfx::AcceleratedWidget HaikuWindowManager::AddWindow(
    HaikuWindow* window,
    HaikuContentView* view) {
  base::AutoLock lock(lock_);
  const gfx::AcceleratedWidget widget = next_widget_++;
  windows_.emplace(widget, Entry{window, view, gfx::Rect()});
  return widget;
}

void HaikuWindowManager::RemoveWindow(gfx::AcceleratedWidget widget,
                                      HaikuWindow* window) {
  base::AutoLock lock(lock_);
  auto found = windows_.find(widget);
  if (found != windows_.end() && found->second.window == window)
    windows_.erase(found);
}

HaikuContentView* HaikuWindowManager::FindView(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  auto found = windows_.find(widget);
  return found == windows_.end() ? nullptr : found->second.view;
}

HaikuWindow* HaikuWindowManager::FindWindow(gfx::AcceleratedWidget widget) {
  base::AutoLock guard(lock_);
  auto it = windows_.find(widget);
  return it == windows_.end() ? nullptr : it->second.window;
}

namespace {

// Written once, from the UI thread, before any window exists; read from the
// UI thread and from BWindow loopers afterwards. A plain pointer rather than
// a lock: nothing reassigns it while windows are alive.
HaikuWindowManager* g_process_manager = nullptr;

}  // namespace

void HaikuWindowManager::SetProcessInstance(HaikuWindowManager* manager) {
  g_process_manager = manager;
}

HaikuWindowManager* GetProcessWindowManager() {
  return g_process_manager;
}

gfx::AcceleratedWidget HaikuWindowManager::SoleWidget() const {
  base::AutoLock guard(lock_);
  if (windows_.size() != 1)
    return gfx::kNullAcceleratedWidget;
  return windows_.begin()->first;
}

size_t HaikuWindowManager::WindowCount() const {
  base::AutoLock guard(lock_);
  return windows_.size();
}

void HaikuWindowManager::SetWindowBounds(gfx::AcceleratedWidget widget,
                                         const gfx::Rect& bounds) {
  base::AutoLock lock(lock_);
  auto found = windows_.find(widget);
  if (found != windows_.end())
    found->second.bounds = bounds;
}

gfx::AcceleratedWidget HaikuWindowManager::WidgetAtScreenPoint(
    const gfx::Point& point) const {
  base::AutoLock lock(lock_);
  // No z-order is tracked, so an overlap resolves arbitrarily. Chromium uses
  // this for drag targeting, where the ambiguity is rare and harmless.
  for (const auto& entry : windows_) {
    if (entry.second.bounds.Contains(point))
      return entry.first;
  }
  return gfx::kNullAcceleratedWidget;
}

void HaikuWindowManager::SetCursorScreenPoint(const gfx::Point& point) {
  base::AutoLock lock(lock_);
  cursor_screen_point_ = point;
}

gfx::Point HaikuWindowManager::CursorScreenPoint() const {
  base::AutoLock lock(lock_);
  return cursor_screen_point_;
}

}  // namespace ui
