#include "ui/ozone/common/stub_client_native_pixmap_factory.h"

namespace ui {

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryHaiku() {
  return CreateStubClientNativePixmapFactory();
}

}  // namespace ui
