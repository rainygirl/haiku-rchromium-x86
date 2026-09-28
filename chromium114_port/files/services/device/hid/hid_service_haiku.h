// Copyright 2026 The Haiku port authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_HAIKU_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_HAIKU_H_

#include "services/device/hid/hid_service.h"

namespace device {

// A HidService that finds nothing, which is what Haiku can honestly report:
// the port builds with use_udev = false and there is no HID backend here.
//
// It has to exist. HidService::Create() falls through to nullptr on a platform
// it does not know, and HidManagerImpl's constructor then does
//
//     DCHECK(hid_service_);
//     hid_service_observation_.Observe(hid_service_.get());
//
// -- a DCHECK, which is compiled out of a release build, followed by
// AddObserver() on a null pointer. x.com's sign-in page asks for the HID
// device list (a passkey is a HID device), and the browser died in
// device::HidService::AddObserver every time.
//
// Modelled on hid_service_fuchsia.{h,cc}, which is the same stub for the same
// reason.
class HidServiceHaiku : public HidService {
 public:
  HidServiceHaiku();
  ~HidServiceHaiku() override;

  HidServiceHaiku(const HidServiceHaiku&) = delete;
  HidServiceHaiku& operator=(const HidServiceHaiku&) = delete;

 private:
  // HidService implementation.
  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

  base::WeakPtrFactory<HidServiceHaiku> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_HAIKU_H_
