#include "haiku_cursor_factory.h"

#include <Bitmap.h>
#include <Cursor.h>

#include <cstring>
#include <utility>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace {

// Haiku's cursor set covers most of what Blink asks for. Where it does not --
// the panning cursors, vertical text, cell -- the nearest sensible Haiku cursor
// is used rather than inventing a bitmap, because a wrong-but-familiar system
// cursor reads better than a foreign-looking drawn one.
BCursorID CursorIdFor(mojom::CursorType type) {
  switch (type) {
    case mojom::CursorType::kPointer:
      return B_CURSOR_ID_SYSTEM_DEFAULT;
    case mojom::CursorType::kCross:
      return B_CURSOR_ID_CROSS_HAIR;
    case mojom::CursorType::kHand:
      return B_CURSOR_ID_FOLLOW_LINK;
    case mojom::CursorType::kIBeam:
      return B_CURSOR_ID_I_BEAM;
    case mojom::CursorType::kWait:
    case mojom::CursorType::kProgress:
      return B_CURSOR_ID_PROGRESS;
    case mojom::CursorType::kHelp:
      return B_CURSOR_ID_HELP;
    case mojom::CursorType::kEastResize:
      return B_CURSOR_ID_RESIZE_EAST;
    case mojom::CursorType::kNorthResize:
      return B_CURSOR_ID_RESIZE_NORTH;
    case mojom::CursorType::kNorthEastResize:
      return B_CURSOR_ID_RESIZE_NORTH_EAST;
    case mojom::CursorType::kNorthWestResize:
      return B_CURSOR_ID_RESIZE_NORTH_WEST;
    case mojom::CursorType::kSouthResize:
      return B_CURSOR_ID_RESIZE_SOUTH;
    case mojom::CursorType::kSouthEastResize:
      return B_CURSOR_ID_RESIZE_SOUTH_EAST;
    case mojom::CursorType::kSouthWestResize:
      return B_CURSOR_ID_RESIZE_SOUTH_WEST;
    case mojom::CursorType::kWestResize:
      return B_CURSOR_ID_RESIZE_WEST;
    case mojom::CursorType::kNorthSouthResize:
    case mojom::CursorType::kRowResize:
      return B_CURSOR_ID_RESIZE_NORTH_SOUTH;
    case mojom::CursorType::kEastWestResize:
    case mojom::CursorType::kColumnResize:
      return B_CURSOR_ID_RESIZE_EAST_WEST;
    case mojom::CursorType::kNorthEastSouthWestResize:
      return B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST;
    case mojom::CursorType::kNorthWestSouthEastResize:
      return B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST;
    case mojom::CursorType::kMove:
    case mojom::CursorType::kMiddlePanning:
    case mojom::CursorType::kEastPanning:
    case mojom::CursorType::kNorthPanning:
    case mojom::CursorType::kNorthEastPanning:
    case mojom::CursorType::kNorthWestPanning:
    case mojom::CursorType::kSouthPanning:
    case mojom::CursorType::kSouthEastPanning:
    case mojom::CursorType::kSouthWestPanning:
    case mojom::CursorType::kWestPanning:
    case mojom::CursorType::kMiddlePanningVertical:
    case mojom::CursorType::kMiddlePanningHorizontal:
      return B_CURSOR_ID_MOVE;
    case mojom::CursorType::kVerticalText:
      return B_CURSOR_ID_I_BEAM_HORIZONTAL;
    case mojom::CursorType::kCell:
      return B_CURSOR_ID_CROSS_HAIR;
    case mojom::CursorType::kContextMenu:
      return B_CURSOR_ID_CONTEXT_MENU;
    case mojom::CursorType::kAlias:
      return B_CURSOR_ID_CREATE_LINK;
    case mojom::CursorType::kCopy:
      return B_CURSOR_ID_COPY;
    case mojom::CursorType::kNoDrop:
    case mojom::CursorType::kNotAllowed:
      return B_CURSOR_ID_NOT_ALLOWED;
    case mojom::CursorType::kNone:
      return B_CURSOR_ID_NO_CURSOR;
    case mojom::CursorType::kZoomIn:
      return B_CURSOR_ID_ZOOM_IN;
    case mojom::CursorType::kZoomOut:
      return B_CURSOR_ID_ZOOM_OUT;
    case mojom::CursorType::kGrab:
      return B_CURSOR_ID_GRAB;
    case mojom::CursorType::kGrabbing:
      return B_CURSOR_ID_GRABBING;
    default:
      return B_CURSOR_ID_SYSTEM_DEFAULT;
  }
}

// Converts a Skia cursor image into a BBitmap. Skia's N32 layout matches
// B_RGBA32 on this little-endian target, so the rows copy straight across.
std::unique_ptr<BBitmap> ToBBitmap(const SkBitmap& bitmap) {
  const int width = bitmap.width();
  const int height = bitmap.height();
  if (width <= 0 || height <= 0)
    return nullptr;

  auto result = std::make_unique<BBitmap>(
      BRect(0, 0, width - 1, height - 1), B_RGBA32);
  if (result->InitCheck() != B_OK)
    return nullptr;

  auto* destination = static_cast<uint8*>(result->Bits());
  const size_t destination_stride = result->BytesPerRow();
  const auto* source = static_cast<const uint8*>(bitmap.getPixels());
  if (source == nullptr)
    return nullptr;
  const size_t source_stride = bitmap.rowBytes();
  const size_t copy_bytes = std::min(destination_stride, source_stride);
  for (int y = 0; y < height; ++y) {
    std::memcpy(destination + y * destination_stride,
                source + y * source_stride, copy_bytes);
  }
  return result;
}

}  // namespace

HaikuCursorFactory::HaikuCursorFactory() = default;

HaikuCursorFactory::~HaikuCursorFactory() {
  for (auto& entry : image_cursor_refs_)
    delete entry.first;
  image_cursor_refs_.clear();
}

base::Optional<PlatformCursor> HaikuCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  const BCursorID id = CursorIdFor(type);
  auto found = default_cursors_.find(static_cast<int>(id));
  if (found == default_cursors_.end()) {
    auto cursor = std::make_unique<BCursor>(id);
    found = default_cursors_
                .emplace(static_cast<int>(id), std::move(cursor))
                .first;
  }
  return static_cast<PlatformCursor>(found->second.get());
}

PlatformCursor HaikuCursorFactory::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  std::unique_ptr<BBitmap> image = ToBBitmap(bitmap);
  if (image == nullptr)
    return nullptr;

  // BCursor copies the bitmap, so the BBitmap does not need to outlive this.
  auto* cursor = new BCursor(image.get(), BPoint(hotspot.x(), hotspot.y()));
  if (cursor->InitCheck() != B_OK) {
    delete cursor;
    return nullptr;
  }

  image_cursor_refs_[cursor] = 1;
  return static_cast<PlatformCursor>(cursor);
}

PlatformCursor HaikuCursorFactory::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms) {
  // app_server has no animated cursor API. Showing the first frame as a static
  // cursor is better than falling back to the default, which would lose the
  // shape entirely.
  if (bitmaps.empty())
    return nullptr;
  return CreateImageCursor(bitmaps.front(), hotspot);
}

void HaikuCursorFactory::RefImageCursor(PlatformCursor cursor) {
  auto found = image_cursor_refs_.find(static_cast<BCursor*>(cursor));
  if (found != image_cursor_refs_.end())
    ++found->second;
}

void HaikuCursorFactory::UnrefImageCursor(PlatformCursor cursor) {
  auto* handle = static_cast<BCursor*>(cursor);
  auto found = image_cursor_refs_.find(handle);
  if (found == image_cursor_refs_.end())
    return;
  if (--found->second > 0)
    return;
  image_cursor_refs_.erase(found);
  delete handle;
}

}  // namespace ui
