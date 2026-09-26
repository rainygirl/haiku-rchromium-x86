// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Haiku's half of CreateSslSystemTrustStore(). The generic file hands Haiku
// to this one rather than to its DummySystemTrustStore, which trusts nothing
// and would fail every TLS handshake.
//
// Haiku has no certificate database to query -- no NSS, no keychain, no
// registry. What it has is one PEM file shipped by the ca_root_certificates
// package, so this reads that file once and keeps the anchors in memory.
// That is the same shape as the Fuchsia branch, which reads
// /config/ssl/cert.pem, and the reason is the same.

#include <FindDirectory.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

// Where the ca_root_certificates package puts the bundle. find_directory()
// rather than a literal /boot: the data directory moves in a non-packaged
// install and on a system booted from another volume.
base::FilePath RootCertsFile() {
  char path[B_PATH_NAME_LENGTH];
  if (find_directory(B_SYSTEM_DATA_DIRECTORY, -1, false, path, sizeof(path)) ==
      B_OK) {
    return base::FilePath(path).Append("ssl/CARootCertificates.pem");
  }
  return base::FilePath("/boot/system/data/ssl/CARootCertificates.pem");
}

class HaikuSystemCerts {
 public:
  HaikuSystemCerts() {
    base::FilePath filename = RootCertsFile();
    std::string certs_file;
    if (!base::ReadFileToString(filename, &certs_file)) {
      LOG(ERROR) << "Can't load root certificates from " << filename;
      return;
    }

    CertificateList certs = X509Certificate::CreateCertificateListFromBytes(
        base::as_bytes(base::make_span(certs_file)),
        X509Certificate::FORMAT_AUTO);

    for (const auto& cert : certs) {
      CertErrors errors;
      auto parsed = ParsedCertificate::Create(
          bssl::UpRef(cert->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
      // A bundle this size picks up certificates the parser rejects -- the
      // Fuchsia branch CHECKs here, which on a general-purpose bundle would
      // take the browser down over one bad anchor. Skip it and keep the rest.
      if (!parsed) {
        LOG(WARNING) << "Skipping a root certificate: "
                     << errors.ToDebugString();
        continue;
      }
      system_trust_store_.AddTrustAnchor(parsed);
    }
  }

  TrustStoreInMemory* system_trust_store() { return &system_trust_store_; }

 private:
  TrustStoreInMemory system_trust_store_;
};

base::LazyInstance<HaikuSystemCerts>::Leaky g_root_certs_haiku =
    LAZY_INSTANCE_INITIALIZER;

class SystemTrustStoreHaiku : public SystemTrustStore {
 public:
  SystemTrustStoreHaiku() = default;

  TrustStore* GetTrustStore() override {
    return g_root_certs_haiku.Get().system_trust_store();
  }

  bool UsesSystemTrustStore() const override { return true; }

  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return g_root_certs_haiku.Get().system_trust_store()->Contains(
        trust_anchor);
  }
};

}  // namespace

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return std::make_unique<SystemTrustStoreHaiku>();
}

}  // namespace net
