#pragma once

#include "jpdf_desk/signing/signature_services.h"

// Portable file-based signing and MuPDF/OpenSSL verification. No native
// certificate store is used, even on Windows or macOS.
SignatureServices openSslSignatureServices();
