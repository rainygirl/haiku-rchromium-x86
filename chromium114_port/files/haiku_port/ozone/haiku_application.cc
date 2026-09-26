#include "haiku_application.h"

#include <Application.h>
#include <Message.h>

namespace ui {

HaikuApplication::HaikuApplication() : thread_(&HaikuApplication::Run, this) {
  std::unique_lock<std::mutex> lock(mutex_);
  ready_condition_.wait(lock, [this] { return ready_; });
}

HaikuApplication::~HaikuApplication() {
  BApplication* application = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    application = application_;
  }
  if (application != nullptr)
    application->PostMessage(B_QUIT_REQUESTED);
  if (thread_.joinable())
    thread_.join();
}

bool HaikuApplication::IsValid() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return application_ != nullptr;
}

void HaikuApplication::Run() {
  BApplication application("application/x-vnd.rchromium-native");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (application.InitCheck() == B_OK)
      application_ = &application;
    ready_ = true;
  }
  ready_condition_.notify_one();
  if (application.InitCheck() == B_OK)
    application.Run();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    application_ = nullptr;
  }
}

}  // namespace ui
