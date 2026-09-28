// Copyright 2026 The Haiku port authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_haiku.h"

#include "base/notreached.h"
#include "services/device/hid/hid_connection.h"

namespace device {

HidServiceHaiku::HidServiceHaiku() {
  // Say the enumeration is over before anyone asks. Without this every
  // GetDevices() sits in pending_enumerations_ and its callback never runs,
  // and a page that waits for the device list waits forever.
  FirstEnumerationComplete();
}

HidServiceHaiku::~HidServiceHaiku() = default;

void HidServiceHaiku::Connect(const std::string& device_id,
                              bool allow_protected_reports,
                              bool allow_fido_reports,
                              ConnectCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(nullptr);
}

base::WeakPtr<HidService> HidServiceHaiku::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
