#include "jpdf_desk/document/pdf_document.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <stdexcept>

namespace {
class TestSigner final : public SigningProvider {
public:
    mutable int calls = 0;
    mutable SigningIdentity received;
    bool fail = true;

    pdf_pkcs7_signer *createSigner(fz_context *, const SigningIdentity &identity) const override
    {
        ++calls;
        received = identity;
        if (fail) throw std::runtime_error("Native key unavailable");
        return nullptr;
    }
};

class TestVerifier final : public VerificationProvider {
public:
    mutable int calls = 0;
    bool fail = true;

    pdf_pkcs7_verifier *createVerifier(fz_context *) const override
    {
        ++calls;
        if (fail) throw std::runtime_error("Native trust store unavailable");
        return nullptr;
    }
    QString trustDescription() const override { return QStringLiteral("Test platform trust store"); }
};

template<typename Action>
bool failsWith(Action action, const char *message)
{
    try { action(); }
    catch (const std::exception &error) { return QByteArray(error.what()).contains(message); }
    return false;
}
}

// This executable intentionally does not link OpenSSL or QtWidgets.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString input = dir.filePath("input.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *source = pdf_create_document(ctx);
    pdf_obj *page = pdf_add_page(ctx, source, {0, 0, 300, 400}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, source, -1, page);
    pdf_drop_obj(ctx, page);
    pdf_save_document(ctx, source, QFile::encodeName(input).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, source);
    fz_drop_context(ctx);

    PdfDocument plain;
    plain.open(input);
    if (plain.canDigitallySign() || plain.canVerifySignatures()
        || !plain.signatureTrustDescription().isEmpty() || plain.render(0).isNull()) return 2;
    const QString output = dir.filePath("output.pdf");
    if (!failsWith([&] { plain.save(output, {}, {}, {}, {}, "opaque-id", "secret"); },
                   "No signing provider")) return 3;
    if (QFile::exists(output) || plain.path() != input) return 4;
    if (!failsWith([&] { plain.checkDigitalSignatures(); }, "No signature verification provider")) return 5;
    plain.save(dir.filePath("unsigned.pdf"), {{0, {{{10, 20, 200, 40}, "Portable", 12}}}});
    if (plain.fields(0).size() != 1) return 6;

    auto signer = std::make_shared<TestSigner>();
    auto verifier = std::make_shared<TestVerifier>();
    PdfDocument injected({signer, verifier});
    injected.open(input);
    if (!injected.canDigitallySign() || !injected.canVerifySignatures()
        || injected.signatureTrustDescription() != verifier->trustDescription()) return 7;
    QFile sentinel(output);
    if (!sentinel.open(QIODevice::WriteOnly) || sentinel.write("unchanged") != 9) return 8;
    sentinel.close();
    auto sign = [&] { injected.saveSnapshot(output, {}, {}, {"native-certificate-id", "secret"}); };
    if (!failsWith(sign, "Native key unavailable") || signer->calls != 1
        || signer->received.identity != "native-certificate-id"
        || signer->received.password != "secret") return 9;
    signer->fail = false;
    if (!failsWith(sign, "no signer") || signer->calls != 2) return 10;
    if (!sentinel.open(QIODevice::ReadOnly) || sentinel.readAll() != "unchanged"
        || injected.path() != input) return 11;
    sentinel.close();

    auto verify = [&] { injected.checkDigitalSignatures(); };
    if (!failsWith(verify, "Native trust store unavailable") || verifier->calls != 1) return 12;
    verifier->fail = false;
    if (!failsWith(verify, "no verifier") || verifier->calls != 2) return 13;
    // C++ provider failures must leave MuPDF's exception stack usable.
    injected.save(dir.filePath("recovered.pdf"), {{0, {{{10, 20, 200, 40}, "Recovered", 12}}}});
    if (injected.fields(0).size() != 1 || injected.render(0).isNull()) return 14;

    PdfDocument signingOnly({signer, {}});
    PdfDocument verificationOnly({{}, verifier});
    if (!signingOnly.canDigitallySign() || signingOnly.canVerifySignatures()
        || verificationOnly.canDigitallySign() || !verificationOnly.canVerifySignatures()) return 15;
    return 0;
}
