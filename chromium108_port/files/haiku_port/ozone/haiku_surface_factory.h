#ifndef RCHROMIUM_HAIKU_SURFACE_FACTORY_H_
#define RCHROMIUM_HAIKU_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class HaikuWindowManager;

class HaikuSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit HaikuSurfaceFactory(HaikuWindowManager* window_manager);
  ~HaikuSurfaceFactory() override;

  // 87 -> 108: gl::GLImplementation became GLImplementationParts, which
  // pairs the implementation with an ANGLE backend. Nothing changes here --
  // this factory allows exactly one implementation, "disabled".
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(
      const gl::GLImplementationParts& implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      absl::optional<gfx::Size> framebuffer_size = absl::nullopt) override;

 private:
  HaikuWindowManager* window_manager_;
};

}  // namespace ui

#endif
