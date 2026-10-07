#pragma once

#include <QString>
#include <memory>

struct fz_context;
struct pdf_pkcs7_signer;
struct pdf_pkcs7_verifier;

// The application chooses an identity; the provider decides how to access it.
// OpenSSL treats identity as a PKCS#12 file path. Native providers can use an
// opaque certificate/key identifier instead. Passwords are never persisted.
struct SigningIdentity {
    QString identity;
    QString password;
};

// Provider factories must translate backend failures to C++ exceptions, not
// let MuPDF longjmp escape. Returned handles are owned by the caller and must
// support pdf_drop_signer/pdf_drop_verifier. Providers must outlive the handles.
class SigningProvider {
public:
    virtual ~SigningProvider() = default;
    virtual pdf_pkcs7_signer *createSigner(fz_context *context,
                                          const SigningIdentity &identity) const = 0;
};

// Separate from key access: verification and certificate trust can use a
// different platform/store than signing. Describe the trust policy for the UI.
class VerificationProvider {
public:
    virtual ~VerificationProvider() = default;
    virtual pdf_pkcs7_verifier *createVerifier(fz_context *context) const = 0;
    virtual QString trustDescription() const = 0;
};

struct SignatureServices {
    std::shared_ptr<const SigningProvider> signing;
    std::shared_ptr<const VerificationProvider> verification;
};
