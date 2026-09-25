#ifndef RCHROMIUM_HAIKU_WINDOW_H_
#define RCHROMIUM_HAIKU_WINDOW_H_

#include <Bitmap.h>
#include <Locker.h>
#include <Rect.h>
#include <View.h>

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "ui/events/event_constants.h"
// EventFlags lives in event_constants.h but EventType has its own header.
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/stub/stub_window.h"

class BWindow;

namespace ui {

class Event;
class HaikuWindowManager;

// The Haiku side of the window. Every override below runs on the BWindow's
// looper thread, not on Chromium's UI thread, so nothing here may touch a
// PlatformWindowDelegate directly. Input is translated on the looper thread
// and handed to the UI thread through `ui_task_runner_`.
// Defined in haiku_beapi_views.cc, which is compiled with RTTI. Returns a
// BWindow subclass whose QuitRequested() asks the Shell to close (through
// HaikuContentView::RequestClose) instead of quitting the BWindow itself,
// which ozone owns and tears down in ~HaikuWindow.
BWindow* CreateBrowserNativeWindow(BRect frame);

class HaikuContentView : public BView {
 public:
  using EventSink = base::RepeatingCallback<void(std::unique_ptr<Event>)>;
  using BoundsSink = base::RepeatingCallback<void(const gfx::Rect&)>;
  using CloseSink = base::RepeatingClosure;

  HaikuContentView();
  ~HaikuContentView() override;

  // Must be called on the Chromium UI thread before the window is shown. The
  // sinks are read from the looper thread afterwards and never reassigned,
  // which is what makes the unsynchronised reads below safe.
  void SetSinks(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                EventSink event_sink,
                BoundsSink bounds_sink,
                CloseSink close_sink);

  // `damage` is the region viz actually repainted, in canvas pixels. Copying
  // and invalidating only that is worth doing here: without a GPU every frame
  // is a full-size memcpy into the BBitmap and then app_server drawing the
  // whole bitmap again, and a blinking text caret alone was repeating both
  // over the entire window several times a second. An empty rect means "all
  // of it", which is what a resize or a first paint wants.
  void Present(const void* pixels,
               int width,
               int height,
               size_t row_bytes,
               const gfx::Rect& damage);

  // The window's close button was pressed. Called on the looper thread; hands
  // the request to the UI thread. Returns false if no sink is wired up yet.
  bool RequestClose();

  // Height in pixels reserved at the top of the window for native chrome that
  // is not part of the web content. Purely geometric: this view knows nothing
  // about what is drawn up there. Read only from AttachedToWindow(), and both
  // that and this setter run with the BWindow locked, which is what keeps the
  // unsynchronised field safe.
  void SetTopInset(int inset) { top_inset_ = inset; }

  // BView, all on the looper thread.
  void Draw(BRect update_rect) override;
  void AttachedToWindow() override;
  void MouseDown(BPoint where) override;
  void MouseUp(BPoint where) override;
  void MouseMoved(BPoint where,
                  uint32 transit,
                  const BMessage* dragged) override;
  void KeyDown(const char* bytes, int32 num_bytes) override;
  void KeyUp(const char* bytes, int32 num_bytes) override;
  void FrameResized(float width, float height) override;
  void MessageReceived(BMessage* message) override;

 private:
  void Post(std::unique_ptr<Event> event);
  void DispatchMouse(EventType type, BPoint where, int32 changed_button);
  void DispatchKey(EventType type, const char* bytes, int32 num_bytes);
  void DispatchWheel(BMessage* message);

  BLocker frame_lock_;
  std::unique_ptr<BBitmap> frame_;

  int top_inset_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  EventSink event_sink_;
  BoundsSink bounds_sink_;
  CloseSink close_sink_;
};

class HaikuWindow : public StubWindow {
 public:
  HaikuWindow(PlatformWindowDelegate* delegate,
              HaikuWindowManager* manager,
              const gfx::Rect& bounds);
  ~HaikuWindow() override;

  // Reserve `inset` pixels at the top of the frame for native chrome. The
  // window grows by that much so the content area keeps the size Chromium
  // asked for: bounds_ stays the content rect throughout, and the BWindow
  // frame is the content rect plus the inset. Call on the UI thread.
  void SetTopInset(int inset);
  int top_inset() const { return top_inset_; }

  // The BWindow this platform window drives. Handed out so that the
  // content_shell platform delegate can add its own BViews above the content
  // view; ozone itself never looks at what goes there, which is why the inset
  // above is expressed in pixels and not in terms of a toolbar.
  //
  // The caller must lock the window before touching it: everything it reaches
  // runs on the looper thread.
  BWindow* GetNativeWindow() { return window_; }

 private:
  // The BWindow frame that gives `content` its full size once the inset is
  // accounted for.
  BRect FrameForContent(const gfx::Rect& content) const;

  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void Activate() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  PlatformWindowState GetPlatformWindowState() const override;

  // Both run on the UI thread, posted from the looper thread. Reached through
  // a weak pointer so that events already in flight when the window is torn
  // down are dropped instead of landing on a freed delegate.
  void OnEventFromLooper(std::unique_ptr<Event> event);
  void OnBoundsFromLooper(const gfx::Rect& bounds);
  void OnCloseFromLooper();

  HaikuWindowManager* manager_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  BWindow* window_ = nullptr;
  HaikuContentView* view_ = nullptr;
  gfx::Rect bounds_;
  int top_inset_ = 0;
  bool visible_ = false;

  base::WeakPtrFactory<HaikuWindow> weak_factory_{this};
};

}  // namespace ui

#endif
