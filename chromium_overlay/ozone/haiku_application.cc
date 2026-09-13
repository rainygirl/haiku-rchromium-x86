#include "haiku_application.h"

#include <Application.h>

namespace ui {

HaikuApplication::HaikuApplication()
    : application_(
          std::make_unique<BApplication>("application/x-vnd.rchromium-native")) {
}

HaikuApplication::~HaikuApplication() = default;

bool HaikuApplication::IsValid() const {
  return application_ != nullptr && application_->InitCheck() == B_OK;
}

}  // namespace ui
