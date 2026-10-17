#include "jpdf_desk/signing/openssl_provider.h"
#include "timestamp_verification.h"
#include "system_certificate_store.h"
#include "jpdf_desk/crypto/detail/openssl_support.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QFile>
#include <QObject>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
extern "C" {
#include <mupdf/helpers/pkcs7-openssl.h>
}
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <stdexcept>
#include <algorithm>

namespace {
class OpenSslSigningProvider final : public SigningProvider {
public:
    pdf_pkcs7_signer *createSigner(fz_context *ctx,
                                  const SigningIdentity &identity) const override
    {
        const QByteArray filename = QFile::encodeName(identity.identity);
        QByteArray secret = identity.password.toUtf8();
        pdf_pkcs7_signer *signer = nullptr;
        try {
            signer = mupdf::call(ctx, [&]() noexcept {
                return pkcs7_openssl_read_pfx(ctx, filename.constData(), secret.constData());
            });
        } catch (...) {
            secret.fill('\0');
            throw;
        }
        secret.fill('\0');
        if (!signer) throw std::runtime_error("Cannot load the signing identity.");
        return signer;
    }
};

void checkSystemChain(PKCS7 *signature, X509 *certificate, SignatureCertificateDetails &result,
                      const QDateTime &now)
{
    using Validation = CertificateChainValidation;
    result.systemChain = Validation::Failed;
    QString error;
    auto store = loadSystemCertificateStore(error);
    if (!store) {
        result.systemChainDescription = error;
        return;
    }
    std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)> context(
        X509_STORE_CTX_new(), X509_STORE_CTX_free);
    auto *chain = PKCS7_type_is_signed(signature) ? signature->d.sign->cert : nullptr;
    if (!context || X509_STORE_CTX_init(context.get(), store.get(), certificate, chain) != 1
        || X509_STORE_CTX_set_purpose(context.get(), X509_PURPOSE_SMIME_SIGN) != 1) {
        result.systemChainDescription = QObject::tr("Cannot initialize system certificate-chain validation.");
        ERR_clear_error();
        return;
    }
    X509_VERIFY_PARAM_set_depth(X509_STORE_CTX_get0_param(context.get()), 16);
    X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(context.get()), now.toSecsSinceEpoch());
    if (X509_verify_cert(context.get()) == 1) {
        result.signerChain = exportValidatedChain(X509_STORE_CTX_get0_chain(context.get()));
        result.systemChain = Validation::Accepted;
        result.systemChainDescription = QObject::tr("Accepted by system/OpenSSL CA roots at current time (revocation not checked).");
    } else {
        const int reason = X509_STORE_CTX_get_error(context.get());
        switch (reason) {
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        case X509_V_ERR_CERT_UNTRUSTED:
            result.systemChain = Validation::Untrusted;
            break;
        default: break; // Expiry, invalid purpose, and backend errors do not prove an untrusted root.
        }
        result.systemChainDescription = QObject::tr("System/OpenSSL chain check failed at depth %1: %2 (code %3).")
            .arg(X509_STORE_CTX_get_error_depth(context.get()))
            .arg(QString::fromLatin1(X509_verify_cert_error_string(reason))).arg(reason);
    }
    ERR_clear_error();
}

SignatureCertificateDetails malformedSignature(const QString &message)
{
    ERR_clear_error();
    SignatureCertificateDetails result;
    result.error = message;
    result.description = message;
    result.systemChain = CertificateChainValidation::Failed;
    result.systemChainDescription = message;
    result.timestamp = {SignatureTimestamp::Status::Failed, {}, message};
    return result;
}

class OpenSslVerificationProvider final : public VerificationProvider {
public:
    pdf_pkcs7_verifier *createVerifier(fz_context *ctx) const override
    {
        auto *verifier = mupdf::call(ctx, [&]() noexcept {
            return pkcs7_openssl_new_verifier(ctx);
        });
        if (!verifier) throw std::runtime_error("Cannot initialize signature verification.");
        return verifier;
    }

    SignatureCertificateDetails certificateDetails(const QByteArray &contents) const override
    {
        return certificateDetails(contents, jpdf::crypto::ValidationTime{});
    }

    SignatureCertificateDetails certificateDetails(const QByteArray &contents,
        const jpdf::crypto::ValidationTime &time) const override
    {
        SignatureCertificateDetails result;
        const auto clockError = time.clockError();
        if (!clockError.isEmpty()) {
            result.systemChain = CertificateChainValidation::Failed;
            result.systemChainDescription = clockError;
            result.description = clockError;
            result.problems << clockError;
            result.timestamp = {SignatureTimestamp::Status::Failed, {}, clockError};
            return result;
        }
        ERR_clear_error();
        const auto *data = reinterpret_cast<const unsigned char *>(contents.constData());
        std::unique_ptr<PKCS7, decltype(&PKCS7_free)> signature(
            d2i_PKCS7(nullptr, &data, contents.size()), PKCS7_free);
        if (!signature)
            return malformedSignature(QObject::tr("Cannot decode the embedded PKCS#7 signature."));
        const auto *end = reinterpret_cast<const unsigned char *>(contents.constData()) + contents.size();
        // PDF Contents commonly reserves space with zero padding. Other trailing
        // data is not part of the parsed signature and must not be silently ignored.
        if (!std::all_of(data, end, [](unsigned char byte) { return byte == 0; }))
            return malformedSignature(QObject::tr("Unexpected data after the embedded PKCS#7 signature."));
        const auto *signerInfos = PKCS7_get_signer_info(signature.get());
        if (!PKCS7_type_is_signed(signature.get()) || !signerInfos || sk_PKCS7_SIGNER_INFO_num(signerInfos) != 1)
            return malformedSignature(QObject::tr("Signature verification requires exactly one PKCS#7 signer."));
        // The stack is ours; the certificates are owned by the PKCS#7 object.
        STACK_OF(X509) *signers = PKCS7_get0_signers(signature.get(), nullptr, 0);
        X509 *certificate = signers && sk_X509_num(signers) == 1
            ? sk_X509_value(signers, 0) : nullptr;
        if (certificate) {
            result.validFrom = jpdf::crypto::detail::asn1DateTime(X509_get0_notBefore(certificate));
            result.validUntil = jpdf::crypto::detail::asn1DateTime(X509_get0_notAfter(certificate));
        }
        sk_X509_free(signers);
        if (!certificate)
            return malformedSignature(QObject::tr("No unique signer certificate found in the signature."));
        result.timestamp = verifySignatureTimestamp(signature.get(), time);
        const auto &now = time.systemUtc();
        checkSystemChain(signature.get(), certificate, result, now);
        if (!result.validFrom.isValid() || !result.validUntil.isValid())
            result.problems << QObject::tr("The certificate validity dates are missing or malformed.");
        else if (result.validFrom > result.validUntil)
            result.problems << QObject::tr("The certificate validity period is malformed: valid-from is after valid-until.");
        else {
            if (now < result.validFrom)
                result.problems << QObject::tr("The signer certificate is not yet valid (valid from %1).")
                    .arg(result.validFrom.toString(Qt::ISODate));
            if (now > result.validUntil)
                result.problems << QObject::tr("The signer certificate has expired (valid until %1).")
                    .arg(result.validUntil.toString(Qt::ISODate));
        }
        result.description = QObject::tr("Certificate dates are checked against the current UTC time. "
            "The certificate's valid-from date is not proof of when it was created. "
            "These date/chain checks are offline; revocation is evaluated separately only on request.");
        return result;
    }

    QString trustDescription() const override
    {
        return QObject::tr("Signer certificates: accepted by either system/OpenSSL CA roots or MuPDF's built-in store. "
                           "Timestamps: system/OpenSSL CA roots only. Linux uses OpenSSL's configured system CA paths; "
                           "Windows/macOS also import native root anchors. This is not the OS/viewer's full trust-policy evaluation. "
                           "Offline by default; optional online revocation uses current evidence only. "
                           "Historical revocation and separate PDF document timestamps are unsupported.");
    }
};
}

SignatureServices openSslSignatureServices()
{
    return {std::make_shared<OpenSslSigningProvider>(),
            std::make_shared<OpenSslVerificationProvider>()};
}
