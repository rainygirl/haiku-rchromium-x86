#include <memory>

#include "haiku_application.h"
#include "haiku_clipboard.h"
#include "haiku_cursor_factory.h"
#include "haiku_screen.h"
#include "haiku_surface_factory.h"
#include "haiku_window.h"
#include "haiku_window_manager.h"

#include <cstdio>
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace {

class HaikuPlatformEventSource : public PlatformEventSource {};

class OzonePlatformHaiku : public OzonePlatform {
 public:
  OzonePlatformHaiku() = default;
  ~OzonePlatformHaiku() override = default;

  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactory* GetCursorFactory() override { return cursor_factory_.get(); }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  PlatformClipboard* GetPlatformClipboard() override {
    return clipboard_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    fprintf(stderr, "[RCH] CreatePlatformWindow called\n");
    return std::make_unique<HaikuWindow>(delegate, window_manager_.get(),
                                         properties.bounds);
  }
  std::unique_ptr<display::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() override {
    return nullptr;
  }
  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<HaikuScreen>(window_manager_.get());
  }
  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodMinimal>(delegate);
  }

 private:
  void InitializeUI(const InitParams& params) override {
    fprintf(stderr, "[RCH] OzonePlatformHaiku::InitializeUI\n");
    application_ = std::make_unique<HaikuApplication>();
    fprintf(stderr, "[RCH] InitializeUI creating window manager\n");
    window_manager_ = std::make_unique<HaikuWindowManager>();
    HaikuWindowManager::SetProcessInstance(window_manager_.get());
    surface_factory_ =
        std::make_unique<HaikuSurfaceFactory>(window_manager_.get());
    event_source_ = std::make_unique<HaikuPlatformEventSource>();
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ = std::make_unique<HaikuCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
    clipboard_ = std::make_unique<HaikuClipboard>();
  }

  void InitializeGPU(const InitParams& params) override {
    fprintf(stderr, "[RCH] InitializeGPU: window_manager_=%p\n",
            (void*)window_manager_.get());
    if (window_manager_ == nullptr) {
      window_manager_ = std::make_unique<HaikuWindowManager>();
      HaikuWindowManager::SetProcessInstance(window_manager_.get());
    }
    if (surface_factory_ == nullptr) {
      surface_factory_ =
          std::make_unique<HaikuSurfaceFactory>(window_manager_.get());
    }
  }

  std::unique_ptr<HaikuApplication> application_;
  std::unique_ptr<HaikuWindowManager> window_manager_;
  std::unique_ptr<HaikuSurfaceFactory> surface_factory_;
  std::unique_ptr<PlatformEventSource> event_source_;
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<HaikuClipboard> clipboard_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformHaiku() {
  return new OzonePlatformHaiku();
}

}  // namespace ui
