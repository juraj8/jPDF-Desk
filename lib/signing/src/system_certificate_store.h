#pragma once

#include <QString>
#include "jpdf_desk/document/certificate_validation.h"
#include <openssl/x509.h>
#include <memory>

using CertificateStore = std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)>;

// OpenSSL default CA paths, plus native root anchors on Windows/macOS.
// This imports roots, not the OS's complete trust-policy evaluator.
CertificateStore loadSystemCertificateStore(QString &error);
ValidatedCertificateChain exportValidatedChain(STACK_OF(X509) *chain);
