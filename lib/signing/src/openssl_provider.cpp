#include "pdf_filler/signing/openssl_provider.h"

#include <QFile>
#include <QObject>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
extern "C" {
#include <mupdf/helpers/pkcs7-openssl.h>
}
#include <stdexcept>

namespace {
class OpenSslSigningProvider final : public SigningProvider {
public:
    pdf_pkcs7_signer *createSigner(fz_context *ctx,
                                  const SigningIdentity &identity) const override
    {
        const QByteArray filename = QFile::encodeName(identity.identity);
        QByteArray secret = identity.password.toUtf8();
        pdf_pkcs7_signer *signer = nullptr;
        int failed = 0;
        fz_var(signer);
        fz_try(ctx) {
            signer = pkcs7_openssl_read_pfx(ctx, filename.constData(), secret.constData());
        }
        fz_catch(ctx) { failed = 1; }
        secret.fill('\0');
        if (failed) throw std::runtime_error(fz_caught_message(ctx));
        if (!signer) throw std::runtime_error("Cannot load the signing identity.");
        return signer;
    }
};

class OpenSslVerificationProvider final : public VerificationProvider {
public:
    pdf_pkcs7_verifier *createVerifier(fz_context *ctx) const override
    {
        pdf_pkcs7_verifier *verifier = nullptr;
        int failed = 0;
        fz_var(verifier);
        fz_try(ctx) { verifier = pkcs7_openssl_new_verifier(ctx); }
        fz_catch(ctx) { failed = 1; }
        if (failed) throw std::runtime_error(fz_caught_message(ctx));
        if (!verifier) throw std::runtime_error("Cannot initialize signature verification.");
        return verifier;
    }

    QString trustDescription() const override
    {
        return QObject::tr("MuPDF's built-in certificate store, not your system or PDF viewer's store. "
                           "This is not a timestamp or online revocation check.");
    }
};
}

SignatureServices openSslSignatureServices()
{
    return {std::make_shared<OpenSslSigningProvider>(),
            std::make_shared<OpenSslVerificationProvider>()};
}
