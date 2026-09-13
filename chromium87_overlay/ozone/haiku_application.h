#ifndef RCHROMIUM_HAIKU_APPLICATION_H_
#define RCHROMIUM_HAIKU_APPLICATION_H_

#include <condition_variable>
#include <mutex>
#include <thread>

class BApplication;

namespace ui {

class HaikuApplication {
 public:
  HaikuApplication();
  ~HaikuApplication();

  HaikuApplication(const HaikuApplication&) = delete;
  HaikuApplication& operator=(const HaikuApplication&) = delete;

  bool IsValid() const;

 private:
  void Run();

  mutable std::mutex mutex_;
  std::condition_variable ready_condition_;
  std::thread thread_;
  BApplication* application_ = nullptr;
  bool ready_ = false;
};

}  // namespace ui

#endif
