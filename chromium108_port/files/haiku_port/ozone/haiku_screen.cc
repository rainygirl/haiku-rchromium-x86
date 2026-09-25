#include "haiku_screen.h"

#include <Deskbar.h>
#include <Screen.h>

#include <algorithm>

#include "haiku_window_manager.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace {

constexpr int64_t kPrimaryDisplayId = 1;

// Haiku beta6 has no per-display scale factor to query, and the target machine
// is a 1600x768 panel driven 1:1, so anything other than 1.0 would be invented.
constexpr float kDisplayScale = 1.0f;

// Used only if BScreen cannot answer, which in practice means the app_server
// connection is not up. A plausible size keeps the browser laying out sanely
// instead of collapsing the way a 1x1 display does.
constexpr int kFallbackWidth = 1024;
constexpr int kFallbackHeight = 768;

gfx::Rect ScreenBounds() {
  BScreen screen(B_MAIN_SCREEN_ID);
  if (!screen.IsValid())
    return gfx::Rect(0, 0, kFallbackWidth, kFallbackHeight);

  const BRect frame = screen.Frame();
  const int width = static_cast<int>(frame.IntegerWidth()) + 1;
  const int height = static_cast<int>(frame.IntegerHeight()) + 1;
  if (width <= 0 || height <= 0)
    return gfx::Rect(0, 0, kFallbackWidth, kFallbackHeight);
  return gfx::Rect(static_cast<int>(frame.left), static_cast<int>(frame.top),
                   width, height);
}

// Subtract the Deskbar from the usable area. Only the edge the Deskbar is
// docked against is trimmed; the check is on which edge its frame touches
// rather than on BDeskbar::Location(), so an unexpected location value cannot
// produce a nonsensical work area.
gfx::Rect WorkAreaFor(const gfx::Rect& bounds) {
  BDeskbar deskbar;
  // There is no IsVisible(); IsRunning() is the correct existence check, and an
  // auto-hidden Deskbar does not occupy the work area.
  if (!deskbar.IsRunning() || deskbar.IsAutoHide())
    return bounds;

  const BRect frame = deskbar.Frame();
  const int left = static_cast<int>(frame.left);
  const int top = static_cast<int>(frame.top);
  const int width = static_cast<int>(frame.IntegerWidth()) + 1;
  const int height = static_cast<int>(frame.IntegerHeight()) + 1;
  if (width <= 0 || height <= 0)
    return bounds;

  gfx::Rect work_area = bounds;
  const bool spans_horizontally = width >= bounds.width();
  if (spans_horizontally) {
    if (top <= bounds.y())
      work_area.Inset(0, height, 0, 0);      // docked at the top
    else
      work_area.Inset(0, 0, 0, height);      // docked at the bottom
  } else {
    if (left <= bounds.x())
      work_area.Inset(width, 0, 0, 0);       // docked on the left
    else
      work_area.Inset(0, 0, width, 0);       // docked on the right
  }

  // A misreported Deskbar must never leave the browser with nothing to draw on.
  if (work_area.width() <= 0 || work_area.height() <= 0)
    return bounds;
  return work_area;
}

}  // namespace

HaikuScreen::HaikuScreen(HaikuWindowManager* manager) : manager_(manager) {
  const gfx::Rect bounds = ScreenBounds();
  display::Display display(kPrimaryDisplayId);
  display.SetScaleAndBounds(kDisplayScale, bounds);
  display.set_work_area(WorkAreaFor(bounds));
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

HaikuScreen::~HaikuScreen() = default;

const std::vector<display::Display>& HaikuScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display HaikuScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return display::Display(kPrimaryDisplayId);
  return *iter;
}

display::Display HaikuScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}

gfx::Point HaikuScreen::GetCursorScreenPoint() const {
  return manager_ == nullptr ? gfx::Point() : manager_->CursorScreenPoint();
}

gfx::AcceleratedWidget HaikuScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  return manager_ == nullptr ? gfx::kNullAcceleratedWidget
                             : manager_->WidgetAtScreenPoint(point);
}

display::Display HaikuScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

display::Display HaikuScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

void HaikuScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void HaikuScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
