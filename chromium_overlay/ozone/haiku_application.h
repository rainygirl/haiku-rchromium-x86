#ifndef RCHROMIUM_NATIVE_CHROMIUM_OVERLAY_OZONE_HAIKU_APPLICATION_H_
#define RCHROMIUM_NATIVE_CHROMIUM_OVERLAY_OZONE_HAIKU_APPLICATION_H_

#include <memory>

class BApplication;

namespace ui {

// Owns the one BApplication required by the Application Kit. Chromium creates
// this on its UI thread before any PlatformWindow instances.
class HaikuApplication final {
 public:
  HaikuApplication();
  ~HaikuApplication();

  HaikuApplication(const HaikuApplication&) = delete;
  HaikuApplication& operator=(const HaikuApplication&) = delete;

  bool IsValid() const;

 private:
  std::unique_ptr<BApplication> application_;
};

}  // namespace ui

#endif
