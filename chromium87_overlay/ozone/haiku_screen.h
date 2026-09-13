#ifndef RCHROMIUM_HAIKU_SCREEN_H_
#define RCHROMIUM_HAIKU_SCREEN_H_

#include <vector>

#include "ui/display/display_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class HaikuWindowManager;

// Reports the real display instead of the 1x1 placeholder that
// ui::HeadlessScreen provides. Bounds come from BScreen, and the work area
// excludes the Deskbar so that a maximised window does not sit underneath it.
class HaikuScreen : public PlatformScreen {
 public:
  explicit HaikuScreen(HaikuWindowManager* manager);
  ~HaikuScreen() override;

  HaikuScreen(const HaikuScreen&) = delete;
  HaikuScreen& operator=(const HaikuScreen&) = delete;

  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  HaikuWindowManager* manager_;
  display::DisplayList display_list_;
};

}  // namespace ui

#endif
