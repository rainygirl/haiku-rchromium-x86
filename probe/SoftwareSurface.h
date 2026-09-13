#ifndef RCHROMIUM_NATIVE_SOFTWARE_SURFACE_H
#define RCHROMIUM_NATIVE_SOFTWARE_SURFACE_H

#include <Bitmap.h>
#include <Locker.h>
#include <View.h>

#include <memory>

// A small direct BeAPI presentation surface. The Chromium Ozone canvas will
// eventually hand its BGRA frame to this object instead of a Qt scene graph.
class SoftwareSurface : public BView {
public:
    explicit SoftwareSurface(const char* name);

    void Draw(BRect updateRect) override;
    void FrameResized(float width, float height) override;
    void MouseDown(BPoint where) override;

private:
    void RebuildFrame(int32 width, int32 height);

    BLocker fLock;
    std::unique_ptr<BBitmap> fFrame;
    uint32 fFrameNumber {0};
};

#endif
