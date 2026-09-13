#include "SoftwareSurface.h"

#include <Autolock.h>

#include <algorithm>
#include <cstdint>

SoftwareSurface::SoftwareSurface(const char* name)
    : BView(name, B_WILL_DRAW | B_FRAME_EVENTS)
{
    SetViewColor(B_TRANSPARENT_COLOR);
}

void SoftwareSurface::Draw(BRect updateRect)
{
    BAutolock lock(&fLock);
    if (fFrame == nullptr)
        RebuildFrame(std::max<int32>(1, static_cast<int32>(Bounds().Width()) + 1),
            std::max<int32>(1, static_cast<int32>(Bounds().Height()) + 1));
    if (fFrame != nullptr)
        DrawBitmap(fFrame.get(), updateRect, updateRect);
}

void SoftwareSurface::FrameResized(float width, float height)
{
    BAutolock lock(&fLock);
    RebuildFrame(std::max<int32>(1, static_cast<int32>(width) + 1),
        std::max<int32>(1, static_cast<int32>(height) + 1));
    Invalidate();
}

void SoftwareSurface::MouseDown(BPoint)
{
    BAutolock lock(&fLock);
    ++fFrameNumber;
    RebuildFrame(std::max<int32>(1, static_cast<int32>(Bounds().Width()) + 1),
        std::max<int32>(1, static_cast<int32>(Bounds().Height()) + 1));
    Invalidate();
}

void SoftwareSurface::RebuildFrame(int32 width, int32 height)
{
    auto frame = std::make_unique<BBitmap>(
        BRect(0, 0, width - 1, height - 1), B_RGB32);
    if (frame->InitCheck() != B_OK)
        return;

    auto* bits = static_cast<uint8*>(frame->Bits());
    const int32 stride = frame->BytesPerRow();
    for (int32 y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<uint32*>(bits + y * stride);
        for (int32 x = 0; x < width; ++x) {
            const bool alternate = (((x / 32) + (y / 32) + fFrameNumber) & 1) != 0;
            const uint8 blue = alternate ? 0xf0 : 0xdd;
            const uint8 green = alternate ? 0xf4 : 0xe8;
            const uint8 red = alternate ? 0xf8 : 0xf0;
            row[x] = 0xff000000u | (static_cast<uint32>(red) << 16)
                | (static_cast<uint32>(green) << 8) | blue;
        }
    }
    fFrame = std::move(frame);
}
