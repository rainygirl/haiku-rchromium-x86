#ifndef RCHROMIUM_HAIKU_CLIPBOARD_H_
#define RCHROMIUM_HAIKU_CLIPBOARD_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/ozone/public/platform_clipboard.h"

class BLooper;

namespace ui {

// PlatformClipboard on top of Haiku's `be_clipboard`.
//
// Haiku's clipboard is synchronous: the data is already in the BMessage when
// the clipboard is locked, so every closure below is invoked before returning
// rather than posted. Callers must not rely on that -- the interface allows
// either -- but it is why there is no pending-request bookkeeping here.
//
// Haiku has no equivalent of the X11 primary selection, so
// ClipboardBuffer::kSelection is unsupported and reported as unavailable.
class HaikuClipboard : public PlatformClipboard {
 public:
  HaikuClipboard();
  ~HaikuClipboard() override;

  HaikuClipboard(const HaikuClipboard&) = delete;
  HaikuClipboard& operator=(const HaikuClipboard&) = delete;

  void OfferClipboardData(ClipboardBuffer buffer,
                          const DataMap& data_map,
                          OfferDataClosure callback) override;
  void RequestClipboardData(ClipboardBuffer buffer,
                            const std::string& mime_type,
                            DataMap* data_map,
                            RequestDataClosure callback) override;
  void GetAvailableMimeTypes(ClipboardBuffer buffer,
                             GetMimeTypesClosure callback) override;
  bool IsSelectionOwner(ClipboardBuffer buffer) override;
  void SetSequenceNumberUpdateCb(SequenceNumberUpdateCb cb) override;
  bool IsSelectionBufferAvailable() const override;

 private:
  // Called on the UI thread when the system clipboard changed under us.
  void OnClipboardChangedExternally();

  SequenceNumberUpdateCb sequence_number_update_cb_;

  // True while the most recent change to the system clipboard is one we made.
  bool is_owner_ = false;

  // A BLooper subclass built with RTTI, from haiku_beapi_looper.h. It must
  // not be built here: a BLooper compiled with -fno-rtti has a null typeinfo
  // pointer, and BApplication::_WindowAt() dynamic_casts every looper in
  // Haiku's global list. It deletes itself when it quits, so this is a raw
  // pointer rather than a unique_ptr that would double-free it.
  BLooper* watcher_ = nullptr;
};

}  // namespace ui

#endif
