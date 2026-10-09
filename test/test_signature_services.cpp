#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/document/detail/mupdf_call.h"
#include "jpdf_desk/signing/pdf_signatures.h"
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

class CountingVerifier final : public VerificationProvider {
    struct Handle : pdf_pkcs7_verifier { int *drops; };
public:
    mutable int drops = 0;
    pdf_pkcs7_verifier *createVerifier(fz_context *) const override
    {
        auto *handle = new Handle{};
        handle->drops = &drops;
        handle->drop = [](fz_context *, pdf_pkcs7_verifier *base) {
            auto *owned = static_cast<Handle *>(base);
            ++*owned->drops;
            delete owned;
        };
        handle->check_digest = [](fz_context *ctx, pdf_pkcs7_verifier *, fz_stream *, unsigned char *, size_t)
                -> pdf_signature_error {
            fz_throw(ctx, FZ_ERROR_FORMAT, "Injected verification failure");
        };
        handle->check_certificate = [](fz_context *ctx, pdf_pkcs7_verifier *, unsigned char *, size_t)
                -> pdf_signature_error {
            fz_throw(ctx, FZ_ERROR_FORMAT, "Injected verification failure");
        };
        handle->get_signatory = [](fz_context *ctx, pdf_pkcs7_verifier *, unsigned char *, size_t)
                -> pdf_pkcs7_distinguished_name * {
            fz_throw(ctx, FZ_ERROR_FORMAT, "Injected verification failure");
        };
        return handle;
    }
    QString trustDescription() const override { return QStringLiteral("Test verifier"); }
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
    const DocumentAnnotations edits{{{0, {{{10, 20, 200, 40}, "Not committed", 12}}}}, {}, {}};
    auto sign = [&] { injected.saveSnapshot(output, edits, {}, {"native-certificate-id", "secret"}); };
    if (!failsWith(sign, "Native key unavailable") || signer->calls != 1
        || signer->received.identity != "native-certificate-id"
        || signer->received.password != "secret") return 9;
    signer->fail = false;
    if (!failsWith(sign, "no signer") || signer->calls != 2) return 10;
    if (!sentinel.open(QIODevice::ReadOnly) || sentinel.readAll() != "unchanged"
        || injected.path() != input || !injected.fields(0).isEmpty()) return 11;
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
    // Exercise verification failures after acquiring the verifier/document,
    // then reuse the same context to detect broken MuPDF exception stacks.
    std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
        fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), fz_drop_context);
    ctx = context.get();
    if (!ctx) return 16;
    const QByteArray inputName = QFile::encodeName(input);
    auto fixture = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
        return pdf_open_document(ctx, inputName.constData());
    }), pdf_drop_document);
    // Prepare conversions outside the longjmp boundary below.
    const QByteArray validName = QFile::encodeName(dir.filePath("fields.pdf"));
    const QByteArray cyclicName = QFile::encodeName(dir.filePath("cyclic.pdf"));
    mupdf::call(ctx, [&]() noexcept {
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, fixture.get()), PDF_NAME(Root));
        pdf_obj *form = pdf_dict_put_dict(ctx, root, PDF_NAME(AcroForm), 1);
        pdf_obj *fields = pdf_dict_put_array(ctx, form, PDF_NAME(Fields), 2);
        pdf_obj *broken = pdf_array_push_dict(ctx, fields, 3);
        pdf_dict_put(ctx, broken, PDF_NAME(FT), PDF_NAME(Sig));
        pdf_dict_put_text_string(ctx, broken, PDF_NAME(T), "Broken");
        pdf_obj *value = pdf_dict_put_dict(ctx, broken, PDF_NAME(V), 1);
        pdf_dict_put_string(ctx, value, PDF_NAME(Contents), "fake", 4);
        pdf_obj *unsignedField = pdf_array_push_dict(ctx, fields, 2);
        pdf_dict_put(ctx, unsignedField, PDF_NAME(FT), PDF_NAME(Sig));
        pdf_dict_put_text_string(ctx, unsignedField, PDF_NAME(T), "Unsigned");
        pdf_save_document(ctx, fixture.get(), validName.constData(), &pdf_default_write_options);
    });
    // An indirect cycle is serializable, but must be rejected by traversal.
    auto cycle = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
        return pdf_add_new_dict(ctx, fixture.get(), 1);
    }), pdf_drop_obj);
    mupdf::call(ctx, [&]() noexcept {
        pdf_obj *kids = pdf_dict_put_array(ctx, cycle.get(), PDF_NAME(Kids), 1);
        pdf_array_push(ctx, kids, cycle.get());
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, fixture.get()), PDF_NAME(Root));
        pdf_obj *form = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
        pdf_array_push(ctx, pdf_dict_get(ctx, form, PDF_NAME(Fields)), cycle.get());
        pdf_save_document(ctx, fixture.get(), cyclicName.constData(), &pdf_default_write_options);
    });
    CountingVerifier counting;
    for (int iteration = 0; iteration < 10; ++iteration) {
        if (!failsWith([&] { verifySavedPdf(ctx, dir.filePath("cyclic.pdf"), counting); },
                       "recursive signature field hierarchy")) return 17;
        const auto statuses = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
        if (statuses.size() != 2 || statuses[0].fieldName != "Broken"
            || !statuses[0].signedField || !statuses[0].error.contains("Injected verification failure")
            || statuses[1].fieldName != "Unsigned" || statuses[1].signedField
            || !statuses[1].error.isEmpty()) return 18;
        if (counting.drops != 2 * (iteration + 1)) return 19;
        if (mupdf::call(ctx, []() noexcept { return 42; }) != 42) return 20;
    }
    return 0;
}
