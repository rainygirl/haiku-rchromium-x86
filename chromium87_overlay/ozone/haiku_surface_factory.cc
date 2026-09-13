#include "haiku_surface_factory.h"

#include "haiku_window.h"
#include "haiku_window_manager.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#include <cstdio>

namespace ui {
namespace {

class HaikuCanvas : public SurfaceOzoneCanvas {
 public:
  HaikuCanvas(HaikuWindowManager* manager, gfx::AcceleratedWidget widget)
      : manager_(manager), widget_(widget) {}
  ~HaikuCanvas() override = default;

  void ResizeCanvas(const gfx::Size& viewport_size) override {
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
    // Rate-limited on purpose: this is called once per composited frame, and
    // the question it answers is only "is anything being drawn at all". A
    // browser that paints once and then stops looks identical from the
    // PresentCanvas side whether the compositor never scheduled another frame
    // or drew one that never reached the BView.
    static int calls = 0;
    if (++calls <= 3 || calls % 60 == 0)
      fprintf(stderr, "[RCH] GetCanvas #%d\n", calls);
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
    if (surface_ == nullptr) {
      fprintf(stderr, "[RCH] PresentCanvas: no surface\n");
      return;
    }
    SkPixmap pixels;
    if (!surface_->peekPixels(&pixels)) {
      fprintf(stderr, "[RCH] PresentCanvas: peekPixels failed\n");
      return;
    }
    const gfx::AcceleratedWidget widget = ResolvedWidget();
    HaikuContentView* view = manager_->FindView(widget);
    fprintf(stderr,
            "[RCH] PresentCanvas %dx%d view=%p widget=%lu (given %lu)\n",
            pixels.width(), pixels.height(), (void*)view,
            (unsigned long)widget, (unsigned long)widget_);
    if (view != nullptr) {
      view->Present(pixels.addr(), pixels.width(), pixels.height(),
                    pixels.rowBytes());
    }
  }

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
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

std::vector<gl::GLImplementation>
HaikuSurfaceFactory::GetAllowedGLImplementations() {
  return {gl::kGLImplementationDisabled};
}

GLOzone* HaikuSurfaceFactory::GetGLOzone(gl::GLImplementation implementation) {
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
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    base::Optional<gfx::Size> framebuffer_size) {
  return nullptr;
}

}  // namespace ui
