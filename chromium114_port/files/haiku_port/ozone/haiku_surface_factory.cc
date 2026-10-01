#include "haiku_surface_factory.h"

#include "haiku_window.h"
#include "haiku_window_manager.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#include <cstdio>
#include <cstdlib>

namespace ui {
namespace {

class HaikuCanvas : public SurfaceOzoneCanvas {
 public:
  HaikuCanvas(HaikuWindowManager* manager, gfx::AcceleratedWidget widget)
      : manager_(manager), widget_(widget) {}
  ~HaikuCanvas() override = default;

  // 87 -> 108: ResizeCanvas gained a scale factor. app_server hands out
  // device pixels and this port reports a scale of 1, so it is ignored.
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override {
    fprintf(stderr, "[RCH] ResizeCanvas %dx%d\n",
            viewport_size.width(), viewport_size.height());
    if (viewport_size.IsEmpty()) {
      surface_.reset();
      return;
    }
    surface_ = SkSurface::MakeRasterN32Premul(viewport_size.width(),
                                              viewport_size.height());
  }

  SkCanvas* GetCanvas() override {
    return surface_ == nullptr ? nullptr : surface_->getCanvas();
  }

  // See HaikuWindowManager::SoleWidget(): viz hands us a null widget, so the
  // lookup has to be corrected before every present. Doing it here rather than
  // once at construction is deliberate -- the window may be registered after
  // the canvas is created, and re-resolving costs a map lookup.
  gfx::AcceleratedWidget ResolvedWidget() const {
    if (widget_ != gfx::kNullAcceleratedWidget)
      return widget_;
    return manager_->SoleWidget();
  }

  void PresentCanvas(const gfx::Rect& damage) override {
    if (surface_ == nullptr)
      return;
    SkPixmap pixels;
    if (!surface_->peekPixels(&pixels))
      return;
    const gfx::AcceleratedWidget widget = ResolvedWidget();
    HaikuContentView* view = manager_->FindView(widget);
    if (view != nullptr) {
      // `damage` was dropped here until 2026-09-24, which made every frame a
      // full-window copy plus a full-window Invalidate -- twice the size of
      // the window in memcpy for a blinking caret.
      view->Present(pixels.addr(), pixels.width(), pixels.height(),
                    pixels.rowBytes(), damage);
    }
  }

  // The display's frame clock. Without a provider viz ticks at 60 Hz, and
  // every tick is a BeginFrame the renderer answers with requestAnimationFrame
  // callbacks, style and layout for whatever animates -- on YouTube that is
  // the player's controls and the live chat, all on the one Atom core the
  // video needs. 30 Hz by default; RCH_FPS=<n> picks another rate.
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    int fps = 30;
    if (const char* env = getenv("RCH_FPS")) {
      const int value = atoi(env);
      if (value >= 10 && value <= 120)
        fps = value;
    }
    return std::make_unique<gfx::FixedVSyncProvider>(
        base::TimeTicks(), base::Seconds(1) / fps);
  }

 private:
  HaikuWindowManager* manager_;
  gfx::AcceleratedWidget widget_;
  sk_sp<SkSurface> surface_;
};

}  // namespace

HaikuSurfaceFactory::HaikuSurfaceFactory(HaikuWindowManager* window_manager)
    : window_manager_(window_manager) {}
HaikuSurfaceFactory::~HaikuSurfaceFactory() = default;

std::vector<gl::GLImplementationParts>
HaikuSurfaceFactory::GetAllowedGLImplementations() {
  return {gl::GLImplementationParts(gl::kGLImplementationDisabled)};
}

GLOzone* HaikuSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas>
HaikuSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  fprintf(stderr, "[RCH] CreateCanvasForWidget widget=%lu manager=%p\n",
          (unsigned long)widget, (void*)window_manager_);
  return std::make_unique<HaikuCanvas>(window_manager_, widget);
}

scoped_refptr<gfx::NativePixmap> HaikuSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  return nullptr;
}

}  // namespace ui
