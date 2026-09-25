#ifndef RCHROMIUM_HAIKU_CURSOR_FACTORY_H_
#define RCHROMIUM_HAIKU_CURSOR_FACTORY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/optional.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class BCursor;
class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

// Produces real Haiku cursors instead of the bitmaps that
// BitmapCursorFactoryOzone hands back.
//
// This exists because BitmapCursorOzone keeps only a bitmap and a hotspot -- it
// discards the CursorType -- so a window given one of those has no way to ask
// app_server for the system cursor that Haiku users expect. Here PlatformCursor
// is a BCursor*, so HaikuWindow can pass it straight to BView::SetViewCursor
// and the pointer over a link is Haiku's own "follow link" cursor rather than a
// Chromium-drawn imitation.
class HaikuCursorFactory : public CursorFactory {
 public:
  HaikuCursorFactory();
  ~HaikuCursorFactory() override;

  HaikuCursorFactory(const HaikuCursorFactory&) = delete;
  HaikuCursorFactory& operator=(const HaikuCursorFactory&) = delete;

  base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  PlatformCursor CreateAnimatedCursor(const std::vector<SkBitmap>& bitmaps,
                                      const gfx::Point& hotspot,
                                      int frame_delay_ms) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;

 private:
  // Default cursors must outlive every user, so they are cached here for the
  // lifetime of the factory and never handed to the refcounting below.
  std::map<int, std::unique_ptr<BCursor>> default_cursors_;

  // Image cursors are refcounted by contract, starting at 1.
  std::map<BCursor*, int> image_cursor_refs_;
};

}  // namespace ui

#endif
