#include "haiku_clipboard.h"

#include <AppDefs.h>
#include <Clipboard.h>
#include <Handler.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "haiku_beapi_looper.h"

namespace ui {
namespace {

// Chromium asks for parameterised types such as "text/plain;charset=utf-8",
// while Haiku applications put plain "text/plain" on the clipboard. Reading
// falls back to the base type, and writing publishes both, so that copy and
// paste work in either direction with native applications.
std::string BaseMimeType(const std::string& mime_type) {
  const size_t separator = mime_type.find(';');
  return separator == std::string::npos ? mime_type
                                        : mime_type.substr(0, separator);
}

bool ReadEntry(const BMessage* clip,
               const std::string& mime_type,
               PlatformClipboard::Data* out) {
  const void* data = nullptr;
  ssize_t length = 0;
  if (clip->FindData(mime_type.c_str(), B_MIME_TYPE, &data, &length) != B_OK)
    return false;
  if (data == nullptr || length < 0)
    return false;
  *out = base::MakeRefCounted<base::RefCountedBytes>(
      static_cast<const unsigned char*>(data), static_cast<size_t>(length));
  return true;
}

}  // namespace

HaikuClipboard::HaikuClipboard() {
  watcher_ = CreateClipboardChangeWatcher(base::BindRepeating(
      &HaikuClipboard::OnClipboardChangedExternally, base::Unretained(this)));
}

HaikuClipboard::~HaikuClipboard() {
  if (watcher_ != nullptr) {
    // The looper deletes itself once it quits, so drop the pointer rather than
    // freeing memory the looper thread still owns.
    StopClipboardChangeWatcher(watcher_);
    watcher_ = nullptr;
  }
}

void HaikuClipboard::OfferClipboardData(ClipboardBuffer buffer,
                                        const DataMap& data_map,
                                        OfferDataClosure callback) {
  if (buffer != ClipboardBuffer::kCopyPaste || be_clipboard == nullptr) {
    std::move(callback).Run();
    return;
  }

  if (be_clipboard->Lock()) {
    be_clipboard->Clear();
    BMessage* clip = be_clipboard->Data();
    if (clip != nullptr) {
      for (const auto& entry : data_map) {
        if (entry.second == nullptr)
          continue;
        // RefCountedBytes declares its own non-const front_as(), which hides
        // the const one in RefCountedMemory, so go through data() instead.
        const std::vector<unsigned char>& bytes = entry.second->data();
        if (bytes.empty())
          continue;
        clip->AddData(entry.first.c_str(), B_MIME_TYPE, bytes.data(),
                      static_cast<ssize_t>(bytes.size()));

        // Also publish under the unparameterised type so that native Haiku
        // applications, which look for "text/plain", can paste this.
        const std::string base_type = BaseMimeType(entry.first);
        if (base_type != entry.first) {
          clip->AddData(base_type.c_str(), B_MIME_TYPE, bytes.data(),
                        static_cast<ssize_t>(bytes.size()));
        }
      }
    }
    be_clipboard->Commit();
    be_clipboard->Unlock();
    is_owner_ = true;
  }

  if (!sequence_number_update_cb_.is_null())
    sequence_number_update_cb_.Run(ClipboardBuffer::kCopyPaste);

  std::move(callback).Run();
}

void HaikuClipboard::RequestClipboardData(ClipboardBuffer buffer,
                                          const std::string& mime_type,
                                          DataMap* data_map,
                                          RequestDataClosure callback) {
  base::Optional<Data> result;

  if (buffer == ClipboardBuffer::kCopyPaste && be_clipboard != nullptr &&
      be_clipboard->Lock()) {
    const BMessage* clip = be_clipboard->Data();
    if (clip != nullptr) {
      Data data;
      const std::string base_type = BaseMimeType(mime_type);
      if (ReadEntry(clip, mime_type, &data) ||
          (base_type != mime_type && ReadEntry(clip, base_type, &data))) {
        result = data;
        if (data_map != nullptr)
          (*data_map)[mime_type] = data;
      }
    }
    be_clipboard->Unlock();
  }

  std::move(callback).Run(result);
}

void HaikuClipboard::GetAvailableMimeTypes(ClipboardBuffer buffer,
                                           GetMimeTypesClosure callback) {
  std::vector<std::string> mime_types;

  if (buffer == ClipboardBuffer::kCopyPaste && be_clipboard != nullptr &&
      be_clipboard->Lock()) {
    const BMessage* clip = be_clipboard->Data();
    if (clip != nullptr) {
      char* name = nullptr;
      type_code type = B_ANY_TYPE;
      for (int32 index = 0;
           clip->GetInfo(B_MIME_TYPE, index, &name, &type) == B_OK; ++index) {
        if (name != nullptr)
          mime_types.emplace_back(name);
      }
    }
    be_clipboard->Unlock();
  }

  std::move(callback).Run(mime_types);
}

bool HaikuClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  return buffer == ClipboardBuffer::kCopyPaste && is_owner_;
}

void HaikuClipboard::SetSequenceNumberUpdateCb(SequenceNumberUpdateCb cb) {
  sequence_number_update_cb_ = std::move(cb);
}

bool HaikuClipboard::IsSelectionBufferAvailable() const {
  // Haiku has no primary-selection equivalent.
  return false;
}

void HaikuClipboard::OnClipboardChangedExternally() {
  is_owner_ = false;
  if (!sequence_number_update_cb_.is_null())
    sequence_number_update_cb_.Run(ClipboardBuffer::kCopyPaste);
}

}  // namespace ui
