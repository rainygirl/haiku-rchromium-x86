#ifndef RCHROMIUM_HAIKU_WINDOW_MANAGER_H_
#define RCHROMIUM_HAIKU_WINDOW_MANAGER_H_

#include <unordered_map>

#include "base/synchronization/lock.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class HaikuContentView;
class HaikuWindow;

// Shared registry of live windows. Everything here is reachable from both the
// Chromium UI thread and the BWindow looper threads, so all of it is under the
// lock.
class HaikuWindowManager {
 public:
  HaikuWindowManager();
  ~HaikuWindowManager();

  gfx::AcceleratedWidget AddWindow(HaikuWindow* window,
                                   HaikuContentView* view);
  void RemoveWindow(gfx::AcceleratedWidget widget, HaikuWindow* window);
  HaikuContentView* FindView(gfx::AcceleratedWidget widget);

  // The window behind a widget. Needed by the content_shell platform delegate,
  // which has an aura WindowTreeHost (and therefore a widget) and needs to
  // reach the BWindow underneath it to hang native chrome off.
  HaikuWindow* FindWindow(gfx::AcceleratedWidget widget);

  // The widget of the only registered window, or kNullAcceleratedWidget when
  // there is not exactly one.
  //
  // A stopgap for the software compositing path. viz creates its
  // SoftwareOutputDevice with gfx::kNullAcceleratedWidget rather than the
  // widget this manager handed out, so PresentCanvas() looks up widget 0, finds
  // no view, and the rendered pixels never reach the BView -- the window stays
  // transparent. Until that handle is plumbed through properly, resolving a
  // null widget to the sole window is correct for content_shell, which has one
  // window per process. It is wrong the moment a second window exists, which is
  // why this returns null rather than guessing in that case.
  gfx::AcceleratedWidget SoleWidget() const;

  // OzonePlatformHaiku creates exactly one of these and registers it here, so
  // that code outside ozone can reach a window by widget without being handed
  // an ozone internal. That is how content/shell attaches its toolbar: see
  // haiku_browser_chrome.h. Null before InitializeUI/InitializeGPU has run.
  static void SetProcessInstance(HaikuWindowManager* manager);

  // Screen-space bounds, kept current so that HaikuScreen can answer hit tests
  // without locking a BWindow from an arbitrary thread.
  void SetWindowBounds(gfx::AcceleratedWidget widget, const gfx::Rect& bounds);
  gfx::AcceleratedWidget WidgetAtScreenPoint(const gfx::Point& point) const;

  // Last cursor position seen by any window, in screen coordinates. Haiku has
  // no cheap global "where is the pointer" call -- BView::GetMouse needs a
  // view and its window -- so this is fed from the mouse events we already
  // handle.
  void SetCursorScreenPoint(const gfx::Point& point);
  gfx::Point CursorScreenPoint() const;

 private:
  struct Entry {
    HaikuWindow* window;
    HaikuContentView* view;
    gfx::Rect bounds;
  };

  mutable base::Lock lock_;
  std::unordered_map<gfx::AcceleratedWidget, Entry> windows_;
  gfx::AcceleratedWidget next_widget_ = 1;
  gfx::Point cursor_screen_point_;
};

// Null until OzonePlatformHaiku has created its manager.
HaikuWindowManager* GetProcessWindowManager();

}  // namespace ui

#endif
