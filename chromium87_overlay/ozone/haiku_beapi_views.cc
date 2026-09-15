// Every class in this file derives from a BeAPI class, and that is the whole
// reason the file exists separately from haiku_window.cc.
//
// Haiku builds libbe.so with RTTI and uses it: BWindow::_FindView() does a
// dynamic_cast<BView*> on the handler for every message the window looper
// delivers, to decide which view a message belongs to. Chromium builds with
// -fno-rtti (build/config/compiler/BUILD.gn leaves use_rtti false), so a BView
// subclass compiled as part of an ordinary Chromium target has no typeinfo for
// that cast to walk -- __dynamic_cast reads a null typeinfo pointer and faults
// at offset 4. That was content_shell dying about a second after its window
// appeared, on the thread named "w>R Chromium", the window's own looper.
//
// Compiling the whole ozone target with RTTI does not work either: the other
// classes there derive from Chromium bases (CursorFactory, PlatformScreen,
// SurfaceFactoryOzone), whose typeinfo does not exist to point at, and the link
// fails with "undefined reference to typeinfo for ui::CursorFactory". So the
// BeAPI-derived classes live here, in their own source_set built with RTTI,
// and everything else stays as it was.

#include "haiku_window.h"

#include <AppDefs.h>
#include <Clipboard.h>
#include <ControlLook.h>
#include <Cursor.h>
#include <Autolock.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <InterfaceDefs.h>
#include <ListItem.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <OutlineListView.h>
#include <Path.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <Window.h>

#include <time.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "haiku_beapi_looper.h"
#include "haiku_browser_chrome.h"
#include "haiku_event_translation.h"
#include "haiku_window_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
namespace {

class BrowserNativeWindow : public BWindow {
 public:
  explicit BrowserNativeWindow(BRect frame)
      : BWindow(frame, "R Chromium", B_TITLED_WINDOW,
                B_ASYNCHRONOUS_CONTROLS) {}

  bool QuitRequested() override {
    fprintf(stderr, "[RCH] BrowserNativeWindow::QuitRequested\n");
    Hide();
    return false;
  }
};

gfx::Point ToPoint(BPoint where) {
  return gfx::Point(static_cast<int>(where.x), static_cast<int>(where.y));
}

// Haiku delivers the button and modifier state in the message being handled
// rather than as arguments, so the hooks below have to go and read it.
void ReadInputState(BView* view, int32* buttons, int32* modifiers) {
  *buttons = 0;
  *modifiers = 0;
  BWindow* window = view->Window();
  BMessage* message = window != nullptr ? window->CurrentMessage() : nullptr;
  if (message == nullptr)
    return;
  message->FindInt32("buttons", buttons);
  message->FindInt32("modifiers", modifiers);
}

// Which button flag to report as the one that changed. Haiku hands us the whole
// mask, so on press the newly pressed button is the one not previously held;
// tracking that precisely buys very little here, and Chromium only uses this to
// attribute the press, so the highest-priority button in the mask is enough.
int ChangedButtonFlag(int32 buttons) {
  if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0)
    return EF_LEFT_MOUSE_BUTTON;
  if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0)
    return EF_RIGHT_MOUSE_BUTTON;
  if ((buttons & B_TERTIARY_MOUSE_BUTTON) != 0)
    return EF_MIDDLE_MOUSE_BUTTON;
  return EF_NONE;
}

}  // namespace

// haiku_window.cc cannot name BrowserNativeWindow: it is defined here so
// that its typeinfo is emitted by an RTTI translation unit.
BWindow* CreateBrowserNativeWindow(BRect frame) {
  return new BrowserNativeWindow(frame);
}

HaikuContentView::HaikuContentView()
    : BView("Chromium canvas", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE) {
  SetViewColor(B_TRANSPARENT_COLOR);
}

HaikuContentView::~HaikuContentView() = default;

void HaikuContentView::SetSinks(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    EventSink event_sink,
    BoundsSink bounds_sink) {
  ui_task_runner_ = std::move(ui_task_runner);
  event_sink_ = std::move(event_sink);
  bounds_sink_ = std::move(bounds_sink);
}

void HaikuContentView::Draw(BRect update_rect) {
  BAutolock lock(&frame_lock_);
  fprintf(stderr, "[RCH] Draw frame=%p\n", (void*)frame_.get());
  if (frame_ != nullptr)
    DrawBitmap(frame_.get(), update_rect, update_rect);
}

void HaikuContentView::AttachedToWindow() {
  BView::AttachedToWindow();

  // The view is constructed with the layout-style BView constructor, so it has
  // no frame of its own until something gives it one. Fill the window and keep
  // following it, otherwise the canvas is zero-sized and nothing is drawn.
  BWindow* window = Window();
  if (window != nullptr) {
    // Sit below whatever chrome occupies the top inset and fill the rest.
    // B_FOLLOW_ALL_SIDES keeps each edge a fixed distance from the matching
    // window edge, so the gap at the top survives every resize without this
    // view having to recompute anything.
    MoveTo(0, top_inset_);
    ResizeTo(window->Bounds().Width(),
             window->Bounds().Height() - top_inset_);
    SetResizingMode(B_FOLLOW_ALL_SIDES);
  }

  // Without focus this view never sees KeyDown.
  MakeFocus(true);
}

void HaikuContentView::Present(const void* pixels,
                               int width,
                               int height,
                               size_t row_bytes) {
  fprintf(stderr, "[RCH] View::Present %dx%d pixels=%p\n", width, height,
          pixels);
  if (pixels == nullptr || width <= 0 || height <= 0)
    return;

  {
    BAutolock lock(&frame_lock_);
    if (frame_ == nullptr || frame_->Bounds().IntegerWidth() + 1 != width ||
        frame_->Bounds().IntegerHeight() + 1 != height) {
      frame_ = std::make_unique<BBitmap>(BRect(0, 0, width - 1, height - 1),
                                         B_RGBA32);
      if (frame_->InitCheck() != B_OK) {
        frame_.reset();
        return;
      }
    }

    auto* destination = static_cast<uint8*>(frame_->Bits());
    const size_t destination_stride = frame_->BytesPerRow();
    const auto* source = static_cast<const uint8*>(pixels);
    const size_t copy_bytes = std::min(destination_stride, row_bytes);
    for (int y = 0; y < height; ++y)
      std::memcpy(destination + y * destination_stride,
                  source + y * row_bytes, copy_bytes);
  }

  BWindow* window = Window();
  if (window != nullptr && window->Lock()) {
    Invalidate();
    window->Unlock();
  }
}

void HaikuContentView::Post(std::unique_ptr<Event> event) {
  if (!ui_task_runner_ || event_sink_.is_null())
    return;
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(event_sink_, std::move(event)));
}

void HaikuContentView::DispatchMouse(EventType type,
                                     BPoint where,
                                     int32 changed_button) {
  int32 buttons = 0;
  int32 modifiers = 0;
  ReadInputState(this, &buttons, &modifiers);

  const gfx::Point location = ToPoint(where);
  // root_location is expected in screen space, and ConvertToScreen is safe
  // here because the window is locked while it dispatches to us.
  const gfx::Point root_location = ToPoint(ConvertToScreen(where));
  auto event = std::make_unique<MouseEvent>(
      type, location, root_location, EventTimeForNow(),
      EventFlagsFromHaiku(modifiers, buttons), changed_button);

  if (type == ET_MOUSE_PRESSED) {
    int32 clicks = 1;
    BWindow* window = Window();
    BMessage* message = window != nullptr ? window->CurrentMessage() : nullptr;
    if (message != nullptr)
      message->FindInt32("clicks", &clicks);
    if (clicks > 1)
      event->SetClickCount(clicks > 2 ? 3 : 2);
  }

  Post(std::move(event));
}

void HaikuContentView::MouseDown(BPoint where) {
  int32 buttons = 0;
  int32 modifiers = 0;
  ReadInputState(this, &buttons, &modifiers);

  // Keep receiving moves once a button is held, so that drags outside the view
  // -- text selection, scrollbar drags -- still reach the renderer.
  SetMouseEventMask(B_POINTER_EVENTS, 0);

  MakeFocus(true);
  DispatchMouse(ET_MOUSE_PRESSED, where, ChangedButtonFlag(buttons));
}

void HaikuContentView::MouseUp(BPoint where) {
  // On release Haiku has already cleared the bit, so the mask no longer names
  // the button that changed. Report the primary button, which is what a
  // release almost always is and what the renderer needs to end a drag.
  DispatchMouse(ET_MOUSE_RELEASED, where, EF_LEFT_MOUSE_BUTTON);
}

void HaikuContentView::MouseMoved(BPoint where,
                                  uint32 transit,
                                  const BMessage* dragged) {
  int32 buttons = 0;
  int32 modifiers = 0;
  ReadInputState(this, &buttons, &modifiers);

  EventType type = ET_MOUSE_MOVED;
  if (transit == B_ENTERED_VIEW)
    type = ET_MOUSE_ENTERED;
  else if (transit == B_EXITED_VIEW)
    type = ET_MOUSE_EXITED;
  else if (buttons != 0)
    type = ET_MOUSE_DRAGGED;

  DispatchMouse(type, where, EF_NONE);
}

void HaikuContentView::DispatchKey(EventType type,
                                   const char* bytes,
                                   int32 num_bytes) {
  int32 raw_char = 0;
  int32 raw_key = 0;
  int32 modifiers = 0;
  BWindow* window = Window();
  BMessage* message = window != nullptr ? window->CurrentMessage() : nullptr;
  if (message != nullptr) {
    message->FindInt32("raw_char", &raw_char);
    message->FindInt32("key", &raw_key);
    message->FindInt32("modifiers", &modifiers);
  }

  const int flags = EventFlagsFromHaiku(modifiers, 0);
  const KeyboardCode key_code = KeyboardCodeFromHaiku(raw_char, raw_key);

  // DomCode and DomKey are deliberately left unset for this first pass. That
  // is enough for shortcuts and for the character path below, but it is why
  // layout-dependent behaviour and IME are still listed as follow-up work.
  Post(std::make_unique<KeyEvent>(type, key_code, flags, EventTimeForNow()));

  if (type != ET_KEY_PRESSED || bytes == nullptr || num_bytes <= 0)
    return;

  // Deliver the text separately. Skip it when a shortcut modifier is held, or
  // Alt+C would both copy and type a character, and skip the control range so
  // that Return and Backspace do not insert anything.
  if ((flags & EF_CONTROL_DOWN) != 0)
    return;

  const base::string16 text =
      base::UTF8ToUTF16(std::string(bytes, static_cast<size_t>(num_bytes)));
  if (text.empty())
    return;
  const base::char16 character = text[0];
  if (character < 0x20 || character == 0x7f)
    return;

  Post(std::make_unique<KeyEvent>(character, key_code, DomCode::NONE, flags,
                                 EventTimeForNow()));
}

void HaikuContentView::KeyDown(const char* bytes, int32 num_bytes) {
  DispatchKey(ET_KEY_PRESSED, bytes, num_bytes);
}

void HaikuContentView::KeyUp(const char* bytes, int32 num_bytes) {
  DispatchKey(ET_KEY_RELEASED, bytes, num_bytes);
}

void HaikuContentView::DispatchWheel(BMessage* message) {
  float delta_x = 0.0f;
  float delta_y = 0.0f;
  message->FindFloat("be:wheel_delta_x", &delta_x);
  message->FindFloat("be:wheel_delta_y", &delta_y);
  if (delta_x == 0.0f && delta_y == 0.0f)
    return;

  BPoint where;
  uint32 buttons = 0;
  GetMouse(&where, &buttons, false);

  // Haiku reports a positive delta when scrolling down; Chromium's offset is
  // positive when the content scrolls up, so both axes are negated.
  const int offset_x =
      -static_cast<int>(delta_x * MouseWheelEvent::kWheelDelta);
  const int offset_y =
      -static_cast<int>(delta_y * MouseWheelEvent::kWheelDelta);

  const gfx::Point location = ToPoint(where);
  const gfx::Point root_location = ToPoint(ConvertToScreen(where));
  Post(std::make_unique<MouseWheelEvent>(
      gfx::Vector2d(offset_x, offset_y), location, root_location,
      EventTimeForNow(),
      EventFlagsFromHaiku(modifiers(), static_cast<int32>(buttons)), EF_NONE));
}

void HaikuContentView::MessageReceived(BMessage* message) {
  if (message->what == B_MOUSE_WHEEL_CHANGED) {
    DispatchWheel(message);
    return;
  }
  BView::MessageReceived(message);
}

void HaikuContentView::FrameResized(float width, float height) {
  BView::FrameResized(width, height);

  if (!ui_task_runner_ || bounds_sink_.is_null())
    return;

  // Safe to read the window here: FrameResized runs on the looper thread with
  // the window already locked.
  BWindow* window = Window();
  const BRect frame =
      window != nullptr ? window->Frame() : BRect(0, 0, width, height);
  const gfx::Rect bounds(static_cast<int>(frame.left),
                         static_cast<int>(frame.top),
                         static_cast<int>(width) + 1,
                         static_cast<int>(height) + 1);

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(bounds_sink_, bounds));
}

namespace {

// Watches for clipboard changes made by other applications. Haiku delivers
// B_CLIPBOARD_CHANGED to a BMessenger, which needs a BHandler on a running
// looper, so this owns a small looper of its own rather than borrowing the
// BApplication's, which lives on another thread and is not ours to extend.
//
// This lived in haiku_clipboard.cc until it was found to be what killed
// content_shell on the first screenshot: `screenshot` sends the application a
// scripting message, BLooper::resolve_specifier() answers it by asking
// BApplication::WindowAt(), and _WindowAt() dynamic_casts every looper in
// Haiku's global list -- including this one, whose typeinfo pointer was null
// because the ozone target is built with -fno-rtti.
class ClipboardChangeWatcher : public BLooper {
 public:
  explicit ClipboardChangeWatcher(base::RepeatingClosure on_changed)
      : BLooper("rchromium clipboard watcher"),
        ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        on_changed_(std::move(on_changed)) {
    Run();
    if (be_clipboard != nullptr)
      be_clipboard->StartWatching(BMessenger(this));
  }

  void Stop() {
    if (be_clipboard != nullptr)
      be_clipboard->StopWatching(BMessenger(this));
    PostMessage(B_QUIT_REQUESTED);
  }

  void MessageReceived(BMessage* message) override {
    if (message->what == B_CLIPBOARD_CHANGED) {
      ui_task_runner_->PostTask(FROM_HERE, on_changed_);
      return;
    }
    BLooper::MessageReceived(message);
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  base::RepeatingClosure on_changed_;
};

}  // namespace

BLooper* CreateClipboardChangeWatcher(base::RepeatingClosure on_changed) {
  return new ClipboardChangeWatcher(std::move(on_changed));
}

void StopClipboardChangeWatcher(BLooper* watcher) {
  static_cast<ClipboardChangeWatcher*>(watcher)->Stop();
}

namespace {

// The toolbar. Icon-only Back, Forward and Reload, then the address field.
//
// Drawn with BControlLook rather than with BButtons carrying bitmaps: it is
// what the rest of Haiku draws its arrows and button frames with, so the
// result follows the user's colour scheme and decorator instead of shipping
// artwork that would look wrong under half of them. It is also why there is
// no icon file to install.
constexpr uint32 kMsgBack = 'rchB';
constexpr uint32 kMsgForward = 'rchF';
constexpr uint32 kMsgReloadStop = 'rchR';
constexpr uint32 kMsgGo = 'rchG';
constexpr uint32 kMsgAddBookmark = 'rchA';
constexpr uint32 kMsgOpenBookmarks = 'rchL';

constexpr float kChromeHeight = 30.0f;
constexpr float kButtonSize = 24.0f;
constexpr float kPadding = 3.0f;

const char kChromeViewName[] = "rchromium chrome";

class ChromeButton : public BView {
 public:
  enum class Glyph { kBack, kForward, kReload, kStop, kStar, kList };

  ChromeButton(BRect frame, const char* name, Glyph glyph, uint32 what)
      : BView(frame, name, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW),
        glyph_(glyph),
        what_(what) {
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
  }

  void SetGlyph(Glyph glyph) {
    if (glyph_ == glyph)
      return;
    glyph_ = glyph;
    Invalidate();
  }

  void SetEnabled(bool enabled) {
    if (enabled_ == enabled)
      return;
    enabled_ = enabled;
    Invalidate();
  }

  void Draw(BRect update_rect) override {
    const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    BRect rect = Bounds();
    uint32 flags = 0;
    if (!enabled_)
      flags |= BControlLook::B_DISABLED;
    if (pressed_)
      flags |= BControlLook::B_ACTIVATED;

    be_control_look->DrawButtonFrame(this, rect, update_rect, base, base,
                                     flags);
    be_control_look->DrawButtonBackground(this, rect, update_rect, base,
                                          flags);

    // rect has been inset by the frame and background passes; centre the
    // glyph in what is left.
    switch (glyph_) {
      case Glyph::kBack:
        be_control_look->DrawArrowShape(this, rect, update_rect, base,
                                        BControlLook::B_LEFT_ARROW, flags);
        break;
      case Glyph::kForward:
        be_control_look->DrawArrowShape(this, rect, update_rect, base,
                                        BControlLook::B_RIGHT_ARROW, flags);
        break;
      case Glyph::kReload:
        DrawReload(rect, base, flags);
        break;
      case Glyph::kStop:
        DrawStop(rect, base, flags);
        break;
      case Glyph::kStar:
        DrawStar(rect, base, flags);
        break;
      case Glyph::kList:
        DrawList(rect, base, flags);
        break;
    }
  }

  void MouseDown(BPoint where) override {
    if (!enabled_)
      return;
    pressed_ = true;
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
    Invalidate();
  }

  void MouseUp(BPoint where) override {
    if (!pressed_)
      return;
    pressed_ = false;
    Invalidate();
    if (enabled_ && Bounds().Contains(where) && Window() != nullptr)
      Window()->PostMessage(what_, Parent());
  }

 private:
  // A three-quarter circle with an arrow head, the usual reload shape. There
  // is no BControlLook primitive for it, so stroke it directly.
  void DrawReload(BRect rect, const rgb_color& base, uint32 flags) {
    PushState();
    rgb_color ink = tint_color(base, B_DARKEN_MAX_TINT);
    if ((flags & BControlLook::B_DISABLED) != 0)
      ink = tint_color(base, B_DARKEN_2_TINT);
    SetHighColor(ink);
    SetPenSize(2.0f);
    SetFlags(Flags() | B_SUBPIXEL_PRECISE);

    const BPoint center(rect.left + rect.Width() / 2.0f,
                        rect.top + rect.Height() / 2.0f);
    const float radius = std::min(rect.Width(), rect.Height()) / 2.0f - 2.0f;
    StrokeArc(center, radius, radius, 300.0f, 300.0f);

    // Arrow head at the open end of the arc, pointing clockwise.
    const float head = radius * 0.55f;
    const BPoint tip(center.x + radius, center.y);
    BPoint points[3] = {
        BPoint(tip.x, tip.y - head),
        BPoint(tip.x - head * 0.8f, tip.y + head * 0.2f),
        BPoint(tip.x + head * 0.8f, tip.y + head * 0.2f),
    };
    FillPolygon(points, 3);
    PopState();
  }

  // Add-bookmark. A five-pointed star, computed rather than drawn from a
  // table so it scales with the button.
  void DrawStar(BRect rect, const rgb_color& base, uint32 flags) {
    PushState();
    SetHighColor(Ink(base, flags));
    const BPoint center(rect.left + rect.Width() / 2.0f,
                        rect.top + rect.Height() / 2.0f);
    const float outer = std::min(rect.Width(), rect.Height()) / 2.0f - 1.0f;
    const float inner = outer * 0.42f;
    BPoint points[10];
    for (int i = 0; i < 10; ++i) {
      // Start at -90 degrees so a point is at the top.
      const double angle = (-90.0 + i * 36.0) * 3.14159265358979 / 180.0;
      const float radius = (i % 2 == 0) ? outer : inner;
      points[i] = BPoint(center.x + radius * static_cast<float>(cos(angle)),
                         center.y + radius * static_cast<float>(sin(angle)));
    }
    FillPolygon(points, 10);
    PopState();
  }

  // Open the bookmark list. Three stacked rules, the usual list glyph.
  void DrawList(BRect rect, const rgb_color& base, uint32 flags) {
    PushState();
    SetHighColor(Ink(base, flags));
    SetPenSize(2.0f);
    BRect inner = rect;
    inner.InsetBy(rect.Width() * 0.22f, rect.Height() * 0.26f);
    const float step = inner.Height() / 2.0f;
    for (int i = 0; i < 3; ++i) {
      const float y = inner.top + i * step;
      StrokeLine(BPoint(inner.left, y), BPoint(inner.right, y));
    }
    PopState();
  }

  static rgb_color Ink(const rgb_color& base, uint32 flags) {
    return (flags & BControlLook::B_DISABLED) != 0
               ? tint_color(base, B_DARKEN_2_TINT)
               : tint_color(base, B_DARKEN_MAX_TINT);
  }

  void DrawStop(BRect rect, const rgb_color& base, uint32 flags) {
    PushState();
    rgb_color ink = tint_color(base, B_DARKEN_MAX_TINT);
    if ((flags & BControlLook::B_DISABLED) != 0)
      ink = tint_color(base, B_DARKEN_2_TINT);
    SetHighColor(ink);
    SetPenSize(2.0f);
    BRect x = rect;
    x.InsetBy(rect.Width() * 0.28f, rect.Height() * 0.28f);
    StrokeLine(BPoint(x.left, x.top), BPoint(x.right, x.bottom));
    StrokeLine(BPoint(x.left, x.bottom), BPoint(x.right, x.top));
    PopState();
  }

  Glyph glyph_;
  uint32 what_;
  bool enabled_ = false;
  bool pressed_ = false;
};

// ------------------------------------------------------------------ bookmarks

struct Bookmark {
  time_t when = 0;
  std::string url;
  std::string title;
};

// Bookmarks on disk, one per line, as
// "<unix seconds>\t<url>\t<title>". A settings *file* rather than a BFS
// attribute directory: this has to survive being copied off a BFS volume,
// which is how the whole port is backed up, and attributes do not.
//
// Tabs and newlines in a title are replaced with spaces on the way in. A
// title containing a tab is not worth a quoting scheme that would make the
// file unreadable in a text editor.
class BookmarkStore {
 public:
  static BookmarkStore& Get() {
    static BookmarkStore* instance = new BookmarkStore();
    return *instance;
  }

  void Add(const std::string& url, const std::string& title) {
    if (url.empty())
      return;
    Bookmark entry;
    entry.when = time(nullptr);
    entry.url = Sanitise(url);
    entry.title = Sanitise(title.empty() ? url : title);

    BAutolock guard(lock_);
    EnsureLoaded();
    // Re-bookmarking a page moves it to today rather than adding a duplicate.
    for (size_t i = 0; i < items_.size(); ++i) {
      if (items_[i].url == entry.url) {
        items_.erase(items_.begin() + i);
        break;
      }
    }
    items_.push_back(entry);
    Save();
  }

  std::vector<Bookmark> All() {
    BAutolock guard(lock_);
    EnsureLoaded();
    return items_;
  }

 private:
  BookmarkStore() : lock_("rchromium bookmarks") {}

  static std::string Sanitise(const std::string& text) {
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i) {
      if (out[i] == '\t' || out[i] == '\n' || out[i] == '\r')
        out[i] = ' ';
    }
    return out;
  }

  static bool SettingsPath(BPath* path) {
    if (find_directory(B_USER_SETTINGS_DIRECTORY, path) != B_OK)
      return false;
    if (path->Append("RChromium") != B_OK)
      return false;
    create_directory(path->Path(), 0755);
    return path->Append("bookmarks") == B_OK;
  }

  void EnsureLoaded() {
    if (loaded_)
      return;
    loaded_ = true;

    BPath path;
    if (!SettingsPath(&path))
      return;
    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK)
      return;
    off_t size = 0;
    if (file.GetSize(&size) != B_OK || size <= 0)
      return;
    std::string text(static_cast<size_t>(size), '\0');
    if (file.Read(&text[0], text.size()) != static_cast<ssize_t>(text.size()))
      return;

    size_t start = 0;
    while (start < text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos)
        end = text.size();
      ParseLine(text.substr(start, end - start));
      start = end + 1;
    }
  }

  void ParseLine(const std::string& line) {
    const size_t first = line.find('\t');
    if (first == std::string::npos)
      return;
    const size_t second = line.find('\t', first + 1);
    if (second == std::string::npos)
      return;
    Bookmark entry;
    entry.when = static_cast<time_t>(atoll(line.substr(0, first).c_str()));
    entry.url = line.substr(first + 1, second - first - 1);
    entry.title = line.substr(second + 1);
    if (!entry.url.empty())
      items_.push_back(entry);
  }

  void Save() {
    BPath path;
    if (!SettingsPath(&path))
      return;
    BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK)
      return;
    for (size_t i = 0; i < items_.size(); ++i) {
      char header[32];
      snprintf(header, sizeof(header), "%lld\t",
               static_cast<long long>(items_[i].when));
      std::string line = header + items_[i].url + "\t" + items_[i].title + "\n";
      file.Write(line.data(), line.size());
    }
  }

  BLocker lock_;
  std::vector<Bookmark> items_;
  bool loaded_ = false;
};

// "Today", "Yesterday", or the date. Grouping is by local calendar day, which
// is what a person means by it, not by 24-hour buckets from now.
std::string DateGroupLabel(time_t when) {
  const time_t now = time(nullptr);
  struct tm entry;
  struct tm today;
  struct tm yesterday;
  const time_t a_day_ago = now - 24 * 60 * 60;
  if (localtime_r(&when, &entry) == nullptr ||
      localtime_r(&now, &today) == nullptr ||
      localtime_r(&a_day_ago, &yesterday) == nullptr) {
    return "Bookmarks";
  }
  if (entry.tm_year == today.tm_year && entry.tm_yday == today.tm_yday)
    return "Today";
  if (entry.tm_year == yesterday.tm_year && entry.tm_yday == yesterday.tm_yday)
    return "Yesterday";
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &entry);
  return buf;
}

bool ContainsNoCase(const std::string& haystack, const std::string& needle) {
  if (needle.empty())
    return true;
  if (needle.size() > haystack.size())
    return false;
  for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    size_t j = 0;
    while (j < needle.size() &&
           tolower(static_cast<unsigned char>(haystack[i + j])) ==
               tolower(static_cast<unsigned char>(needle[j]))) {
      ++j;
    }
    if (j == needle.size())
      return true;
  }
  return false;
}

class BookmarkItem : public BStringItem {
 public:
  BookmarkItem(const std::string& label, const std::string& url)
      : BStringItem(label.c_str(), 1, true), url_(url) {}

  const std::string& url() const { return url_; }

 private:
  std::string url_;
};

constexpr uint32 kMsgBookmarkSearch = 'rchS';
constexpr uint32 kMsgBookmarkOpen = 'rchO';

class BookmarksWindow : public BWindow {
 public:
  explicit BookmarksWindow(HaikuBrowserChromeClient* client)
      : BWindow(BRect(120, 120, 620, 520), "Bookmarks", B_TITLED_WINDOW,
                B_ASYNCHRONOUS_CONTROLS),
        client_(client) {
    BRect bounds = Bounds();

    BRect search_frame(kPadding, kPadding, bounds.right - kPadding,
                       kPadding + 22);
    search_ = new BTextControl(search_frame, "search", nullptr, "",
                               new BMessage(kMsgBookmarkSearch),
                               B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
    search_->SetDivider(0.0f);
    search_->SetModificationMessage(new BMessage(kMsgBookmarkSearch));
    AddChild(search_);

    BRect list_frame(kPadding, search_frame.bottom + kPadding,
                     bounds.right - kPadding - B_V_SCROLL_BAR_WIDTH,
                     bounds.bottom - kPadding);
    list_ = new BOutlineListView(list_frame, "bookmarks",
                                 B_SINGLE_SELECTION_LIST,
                                 B_FOLLOW_ALL_SIDES);
    list_->SetInvocationMessage(new BMessage(kMsgBookmarkOpen));
    AddChild(new BScrollView("scroller", list_, B_FOLLOW_ALL_SIDES, 0, false,
                             true));

    Rebuild();
  }

  bool QuitRequested() override {
    Hide();
    return false;
  }

  void MessageReceived(BMessage* message) override {
    switch (message->what) {
      case kMsgBookmarkSearch:
        Rebuild();
        return;
      case kMsgBookmarkOpen:
        OpenSelection();
        return;
      default:
        BWindow::MessageReceived(message);
    }
  }

  void Refresh() {
    if (!Lock())
      return;
    Rebuild();
    Unlock();
  }

 private:
  void OpenSelection() {
    const int32 index = list_->CurrentSelection();
    if (index < 0)
      return;
    BookmarkItem* item = dynamic_cast<BookmarkItem*>(list_->ItemAt(index));
    if (item != nullptr)
      client_->OnNavigate(item->url());
  }

  static bool Newer(const Bookmark& a, const Bookmark& b) {
    return a.when > b.when;
  }

  void Rebuild() {
    search_->SetTarget(this);
    list_->SetTarget(this);

    while (BListItem* item = list_->RemoveItem(int32(0)))
      delete item;

    std::vector<Bookmark> entries = BookmarkStore::Get().All();
    std::sort(entries.begin(), entries.end(), Newer);

    const std::string query =
        search_->Text() != nullptr ? std::string(search_->Text()) : "";

    std::string open_group;
    for (size_t i = 0; i < entries.size(); ++i) {
      const Bookmark& entry = entries[i];
      if (!ContainsNoCase(entry.title, query) &&
          !ContainsNoCase(entry.url, query)) {
        continue;
      }
      const std::string group = DateGroupLabel(entry.when);
      if (group != open_group) {
        // Expanded by default: the third argument is `expanded`, and passing
        // false left every date group collapsed, so opening the window showed
        // only a lone "Today" row and no bookmarks until the user clicked it.
        list_->AddItem(new BStringItem(group.c_str(), 0, true));
        open_group = group;
      }
      const std::string label =
          entry.title.empty() ? entry.url : entry.title + "  -  " + entry.url;
      list_->AddItem(new BookmarkItem(label, entry.url));
    }
  }

  HaikuBrowserChromeClient* client_;
  BTextControl* search_ = nullptr;
  BOutlineListView* list_ = nullptr;
};

class BrowserChromeView : public BView {
 public:
  BrowserChromeView(BRect frame, HaikuBrowserChromeClient* client)
      : BView(frame, kChromeViewName, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
              B_WILL_DRAW | B_FRAME_EVENTS),
        client_(client) {
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

    float x = kPadding;
    back_ = AddButton(&x, "back", ChromeButton::Glyph::kBack, kMsgBack);
    forward_ =
        AddButton(&x, "forward", ChromeButton::Glyph::kForward, kMsgForward);
    reload_ =
        AddButton(&x, "reload", ChromeButton::Glyph::kReload, kMsgReloadStop);
    reload_->SetEnabled(true);

    // Bookmark buttons sit at the right-hand end, so the address field is
    // between the navigation controls and them. Their frames are fixed up in
    // Layout() along with the field.
    star_ = new ChromeButton(BRect(0, 0, kButtonSize - 1, kButtonSize - 1),
                             "add bookmark", ChromeButton::Glyph::kStar,
                             kMsgAddBookmark);
    star_->SetEnabled(true);
    AddChild(star_);
    bookmarks_ = new ChromeButton(BRect(0, 0, kButtonSize - 1, kButtonSize - 1),
                                  "bookmarks", ChromeButton::Glyph::kList,
                                  kMsgOpenBookmarks);
    bookmarks_->SetEnabled(true);
    AddChild(bookmarks_);

    address_ = new BTextControl(AddressFrame(x), "address", nullptr, "",
                                new BMessage(kMsgGo),
                                B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
    address_->SetDivider(0.0f);
    AddChild(address_);
    address_left_ = x;
    Layout(Bounds().Width());
  }

  void AttachedToWindow() override {
    BView::AttachedToWindow();
    address_->SetTarget(this);
  }

  void FrameResized(float width, float height) override {
    BView::FrameResized(width, height);
    Layout(width);
  }

  void MessageReceived(BMessage* message) override {
    switch (message->what) {
      case kMsgBack:
        client_->OnBack();
        return;
      case kMsgForward:
        client_->OnForward();
        return;
      case kMsgReloadStop:
        client_->OnReloadOrStop();
        return;
      case kMsgGo:
        client_->OnNavigate(address_->Text() != nullptr ? address_->Text()
                                                        : "");
        return;
      case kMsgAddBookmark:
        BookmarkStore::Get().Add(current_url_, current_title_);
        if (bookmarks_window_ != nullptr)
          bookmarks_window_->Refresh();
        return;
      case kMsgOpenBookmarks:
        ShowBookmarks();
        return;
      default:
        BView::MessageReceived(message);
    }
  }

  void SetAddress(const char* url) {
    current_url_ = url != nullptr ? url : "";
    // Do not fight the user: a page that finishes loading while they are
    // typing must not replace what is in the field.
    if (address_->TextView() != nullptr && address_->TextView()->IsFocus())
      return;
    address_->SetText(url);
  }

  void SetPageTitle(const char* title) {
    current_title_ = title != nullptr ? title : "";
  }

  void SetNavState(bool can_go_back, bool can_go_forward, bool is_loading) {
    back_->SetEnabled(can_go_back);
    forward_->SetEnabled(can_go_forward);
    reload_->SetGlyph(is_loading ? ChromeButton::Glyph::kStop
                                 : ChromeButton::Glyph::kReload);
  }

 private:
  void ShowBookmarks() {
    if (bookmarks_window_ == nullptr)
      bookmarks_window_ = new BookmarksWindow(client_);
    else
      bookmarks_window_->Refresh();
    if (bookmarks_window_->Lock()) {
      if (bookmarks_window_->IsHidden())
        bookmarks_window_->Show();
      bookmarks_window_->Activate(true);
      bookmarks_window_->Unlock();
    }
  }

  // The address field takes whatever is left between the navigation buttons
  // and the two bookmark buttons at the right edge.
  void Layout(float width) {
    const float top = (kChromeHeight - kButtonSize) / 2.0f;
    const float right_block = 2 * (kButtonSize + kPadding);
    bookmarks_->MoveTo(width - kButtonSize - kPadding, top);
    star_->MoveTo(width - 2 * kButtonSize - 2 * kPadding, top);
    const float address_width =
        width - address_left_ - kPadding - right_block;
    address_->ResizeTo(address_width > 40.0f ? address_width : 40.0f,
                       address_->Bounds().Height());
  }

  ChromeButton* AddButton(float* x,
                          const char* name,
                          ChromeButton::Glyph glyph,
                          uint32 what) {
    const float top = (kChromeHeight - kButtonSize) / 2.0f;
    BRect frame(*x, top, *x + kButtonSize - 1, top + kButtonSize - 1);
    ChromeButton* button = new ChromeButton(frame, name, glyph, what);
    AddChild(button);
    *x += kButtonSize + kPadding;
    return button;
  }

  BRect AddressFrame(float left) const {
    BRect frame(left, kPadding, Bounds().right - kPadding,
                kChromeHeight - kPadding - 1);
    return frame;
  }

  HaikuBrowserChromeClient* client_;
  ChromeButton* back_ = nullptr;
  ChromeButton* forward_ = nullptr;
  ChromeButton* reload_ = nullptr;
  ChromeButton* star_ = nullptr;
  ChromeButton* bookmarks_ = nullptr;
  BTextControl* address_ = nullptr;
  float address_left_ = 0.0f;

  // What the shell last told us we are showing, which is what the star
  // bookmarks. Kept here rather than asked for on demand: the button is
  // clicked on the looper thread and the WebContents may only be touched on
  // the UI thread.
  std::string current_url_;
  std::string current_title_;

  // Created on first use and then kept: a bookmark window that is closed is
  // hidden rather than quit, so its search text and scroll position survive.
  BookmarksWindow* bookmarks_window_ = nullptr;
};

BrowserChromeView* ChromeOf(BWindow* window) {
  BView* view = window->FindView(kChromeViewName);
  // Named uniquely by this file and added only by AttachBrowserChrome(), so a
  // hit is always ours.
  return static_cast<BrowserChromeView*>(view);
}

}  // namespace

namespace {

// The BWindow behind `widget`, or behind the process's only window when the
// caller passes a null widget -- which content/shell does, because that is
// still all viz gives it.
BWindow* NativeWindowFor(gfx::AcceleratedWidget widget) {
  HaikuWindowManager* manager = GetProcessWindowManager();
  if (manager == nullptr)
    return nullptr;
  if (widget == gfx::kNullAcceleratedWidget)
    widget = manager->SoleWidget();
  HaikuWindow* window = manager->FindWindow(widget);
  return window == nullptr ? nullptr : window->GetNativeWindow();
}

}  // namespace

int AttachBrowserChrome(gfx::AcceleratedWidget widget,
                        HaikuBrowserChromeClient* client) {
  HaikuWindowManager* manager = GetProcessWindowManager();
  if (manager == nullptr)
    return 0;
  if (widget == gfx::kNullAcceleratedWidget)
    widget = manager->SoleWidget();
  HaikuWindow* window = manager->FindWindow(widget);
  if (window == nullptr)
    return 0;
  BWindow* native = window->GetNativeWindow();
  if (native == nullptr || !native->Lock())
    return 0;
  BRect frame = native->Bounds();
  frame.bottom = frame.top + kChromeHeight - 1;
  native->AddChild(new BrowserChromeView(frame, client));
  native->Unlock();

  // After unlocking: SetTopInset() grows the window and moves the content view
  // itself, taking the lock again as it goes.
  window->SetTopInset(static_cast<int>(kChromeHeight));
  return static_cast<int>(kChromeHeight);
}

void SetBrowserChromeAddress(gfx::AcceleratedWidget widget,
                             const std::string& url) {
  BWindow* window = NativeWindowFor(widget);
  if (window == nullptr || !window->Lock())
    return;
  BrowserChromeView* chrome = ChromeOf(window);
  if (chrome != nullptr)
    chrome->SetAddress(url.c_str());
  window->Unlock();
}

void SetBrowserChromeTitle(gfx::AcceleratedWidget widget,
                           const std::string& title) {
  BWindow* window = NativeWindowFor(widget);
  if (window == nullptr || !window->Lock())
    return;
  BrowserChromeView* chrome = ChromeOf(window);
  if (chrome != nullptr)
    chrome->SetPageTitle(title.c_str());
  window->Unlock();
}

void SetBrowserChromeNavState(gfx::AcceleratedWidget widget,
                              bool can_go_back,
                              bool can_go_forward,
                              bool is_loading) {
  BWindow* window = NativeWindowFor(widget);
  if (window == nullptr || !window->Lock())
    return;
  BrowserChromeView* chrome = ChromeOf(window);
  if (chrome != nullptr)
    chrome->SetNavState(can_go_back, can_go_forward, is_loading);
  window->Unlock();
}

}  // namespace ui
