#pragma once

#include "jpdf_desk/document/signature_timestamp.h"
#include "jpdf_desk/document/certificate_validation.h"

#include "jpdf_desk/crypto/validation_time.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
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
struct SignatureCertificateDetails {
    // An unreadable/unsupported signature envelope must not fall back to another
    // chain policy. This does not erase an independently computed digest result.
    QString error;
    QDateTime validFrom;
    QDateTime validUntil;
    QStringList problems;
    QString description;
    SignatureTimestamp timestamp;
    CertificateChainValidation systemChain = CertificateChainValidation::NotChecked;
    QString systemChainDescription;
    ValidatedCertificateChain signerChain;
};

// Verification may run on a worker thread. Implementations must not access
// GUI objects and must keep per-operation mutable state in the returned handle.
class VerificationProvider {
public:
    virtual ~VerificationProvider() = default;
    // Create a cryptographic verifier without imposing a clock/trust decision;
    // the PDF layer gates certificate/metadata callbacks separately.
    virtual pdf_pkcs7_verifier *createVerifier(fz_context *context) const = 0;
    virtual QString trustDescription() const = 0;
    // Optional backend-specific metadata and additional system-chain validation.
    // The application combines this with the verifier's built-in chain policy.
    virtual SignatureCertificateDetails certificateDetails(const QByteArray &) const { return {}; }
    // Operation time is shared across fields and backend checks. Legacy providers
    // can retain the metadata-only overload; time-aware providers override this one.
    virtual SignatureCertificateDetails certificateDetails(const QByteArray &contents,
        const jpdf::crypto::ValidationTime &) const { return certificateDetails(contents); }
};

struct SignatureServices {
    std::shared_ptr<const SigningProvider> signing;
    std::shared_ptr<const VerificationProvider> verification;
};
