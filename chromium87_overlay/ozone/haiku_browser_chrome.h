#ifndef RCHROMIUM_HAIKU_BROWSER_CHROME_H_
#define RCHROMIUM_HAIKU_BROWSER_CHROME_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

namespace ui {

// What the native toolbar needs from whoever owns the web contents.
//
// Implemented in content/shell, which cannot define the toolbar itself: every
// BView subclass must be compiled with RTTI (see haiku_beapi_looper.h), and
// content/shell is an ordinary -fno-rtti target. So the views live in
// haiku_beapi_views.cc and talk back through this plain interface, which has
// no BeAPI base and is safe to implement anywhere.
//
// Every method is called on the BWindow's looper thread. Implementations must
// hop to the UI thread themselves before touching a WebContents.
class HaikuBrowserChromeClient {
 public:
  virtual void OnBack() = 0;
  virtual void OnForward() = 0;
  // Reloads, or stops if a load is in progress -- the toolbar has one button
  // for both and tells them apart by the state it was last given.
  virtual void OnReloadOrStop() = 0;
  virtual void OnNavigate(const std::string& text) = 0;

 protected:
  ~HaikuBrowserChromeClient() = default;
};

// Everything below is addressed by widget rather than by BWindow so that
// content/shell never names a BeAPI type. Pass kNullAcceleratedWidget to mean
// "the only window in this process", which is what viz still hands out (see
// HaikuWindowManager::SoleWidget).

// Builds the toolbar, adds it to the window behind `widget`, and reserves room
// for it so the web content starts below. Returns its height in pixels, or 0
// if there is no such window. Call on the UI thread.
int AttachBrowserChrome(gfx::AcceleratedWidget widget,
                        HaikuBrowserChromeClient* client);

// Both take the window lock themselves, so either thread may call them, and
// both do nothing if the window has no toolbar.
void SetBrowserChromeAddress(gfx::AcceleratedWidget widget,
                             const std::string& url);
// The page title, which is what the star button files a bookmark under.
void SetBrowserChromeTitle(gfx::AcceleratedWidget widget,
                           const std::string& title);
void SetBrowserChromeNavState(gfx::AcceleratedWidget widget,
                              bool can_go_back,
                              bool can_go_forward,
                              bool is_loading);

}  // namespace ui

#endif
