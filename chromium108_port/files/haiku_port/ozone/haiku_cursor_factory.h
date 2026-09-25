#ifndef RCHROMIUM_HAIKU_CURSOR_FACTORY_H_
#define RCHROMIUM_HAIKU_CURSOR_FACTORY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class BCursor;
class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

// Produces real Haiku cursors instead of the bitmaps that
// BitmapCursorFactory hands back.
//
// 87 -> 108: PlatformCursor stopped being a typedef -- it was a void*, and
// here a BCursor* -- and became a ref-counted base class. So the BCursor now
// lives inside a HaikuPlatformCursor, the two Ref/UnrefImageCursor overrides
// are gone, and with them the hand-written refcount table this class used to
// keep. CreateImageCursor and CreateAnimatedCursor also gained a CursorType
// parameter and the animated one takes a TimeDelta rather than milliseconds.
//
// This exists because BitmapCursorOzone keeps only a bitmap and a hotspot -- it
// discards the CursorType -- so a window given one of those has no way to ask
// app_server for the system cursor that Haiku users expect. Here PlatformCursor
// is a BCursor*, so HaikuWindow can pass it straight to BView::SetViewCursor
// and the pointer over a link is Haiku's own "follow link" cursor rather than a
// Chromium-drawn imitation.
// Unwraps the BCursor a HaikuCursorFactory handed out. Returns nullptr for a
// null cursor, which SetViewCursor takes to mean "the default".
const BCursor* BCursorFromPlatformCursor(PlatformCursor* cursor);

class HaikuCursorFactory : public CursorFactory {
 public:
  HaikuCursorFactory();
  ~HaikuCursorFactory() override;

  HaikuCursorFactory(const HaikuCursorFactory&) = delete;
  HaikuCursorFactory& operator=(const HaikuCursorFactory&) = delete;

  scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  scoped_refptr<PlatformCursor> CreateImageCursor(
      mojom::CursorType type,
      const SkBitmap& bitmap,
      const gfx::Point& hotspot) override;
  scoped_refptr<PlatformCursor> CreateAnimatedCursor(
      mojom::CursorType type,
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      base::TimeDelta frame_delay) override;

 private:
  // Default cursors are cached for the lifetime of the factory: there are a
  // few dozen of them, they are cheap, and app_server owns the real cursor
  // data anyway.
  std::map<int, scoped_refptr<PlatformCursor>> default_cursors_;
};

}  // namespace ui

#endif
