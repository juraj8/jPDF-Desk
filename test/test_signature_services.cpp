#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/document/detail/mupdf_call.h"
#include "jpdf_desk/signing/pdf_signatures.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <stdexcept>
#include <iostream>

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
    struct Handle : pdf_pkcs7_verifier { int *drops; const CountingVerifier *owner; };
public:
    mutable int drops = 0;
    mutable int creations = 0;
    mutable int digestCalls = 0;
    mutable int certificateCalls = 0;
    pdf_signature_error digestResult = PDF_SIGNATURE_ERROR_OKAY;
    pdf_signature_error certificateResult = PDF_SIGNATURE_ERROR_UNKNOWN;
    bool failDigest = false;
    bool failMetadata = false;
    bool failIdentity = true;
    bool failCertificate = true;
    mutable QVector<QDateTime> metadataTimes;
    SignatureCertificateDetails certificateDetails(const QByteArray &,
        const jpdf::crypto::ValidationTime &time) const override
    {
        metadataTimes.push_back(time.systemUtc());
        if (failMetadata) throw std::runtime_error("Injected metadata failure");
        return {};
    }
    pdf_pkcs7_verifier *createVerifier(fz_context *) const override
    {
        ++creations;
        auto *handle = new Handle{};
        handle->drops = &drops;
        handle->owner = this;
        handle->drop = [](fz_context *, pdf_pkcs7_verifier *base) {
            auto *owned = static_cast<Handle *>(base);
            ++*owned->drops;
            delete owned;
        };
        handle->check_digest = [](fz_context *ctx, pdf_pkcs7_verifier *base, fz_stream *, unsigned char *, size_t)
                -> pdf_signature_error {
            const auto *owner = static_cast<Handle *>(base)->owner;
            ++owner->digestCalls;
            if (owner->failDigest) fz_throw(ctx, FZ_ERROR_FORMAT, "Injected digest failure");
            return owner->digestResult;
        };
        handle->check_certificate = [](fz_context *ctx, pdf_pkcs7_verifier *base, unsigned char *, size_t)
                -> pdf_signature_error {
            const auto *owner = static_cast<Handle *>(base)->owner;
            ++owner->certificateCalls;
            if (owner->failCertificate) fz_throw(ctx, FZ_ERROR_FORMAT, "Injected certificate failure");
            return owner->certificateResult;
        };
        handle->get_signatory = [](fz_context *ctx, pdf_pkcs7_verifier *base, unsigned char *, size_t)
                -> pdf_pkcs7_distinguished_name * {
            if (!static_cast<Handle *>(base)->owner->failIdentity) return nullptr;
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
    const QByteArray invalidKidsName = QFile::encodeName(dir.filePath("invalid-kids.pdf"));
    const QByteArray invalidFieldName = QFile::encodeName(dir.filePath("invalid-field.pdf"));
    const QByteArray deepName = QFile::encodeName(dir.filePath("deep.pdf"));
    const QByteArray invalidFormName = QFile::encodeName(dir.filePath("invalid-form.pdf"));
    mupdf::call(ctx, [&]() noexcept {
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, fixture.get()), PDF_NAME(Root));
        pdf_obj *form = pdf_dict_put_dict(ctx, root, PDF_NAME(AcroForm), 1);
        pdf_obj *fields = pdf_dict_put_array(ctx, form, PDF_NAME(Fields), 2);
        pdf_obj *broken = pdf_array_push_dict(ctx, fields, 3);
        pdf_dict_put(ctx, broken, PDF_NAME(FT), PDF_NAME(Sig));
        pdf_dict_put_text_string(ctx, broken, PDF_NAME(T), "Broken");
        pdf_obj *value = pdf_add_new_dict(ctx, fixture.get(), 1);
        pdf_dict_put(ctx, broken, PDF_NAME(V), value);
        pdf_dict_put_string(ctx, value, PDF_NAME(Contents), "fake", 4);
        pdf_obj *second = pdf_array_push_dict(ctx, fields, 3);
        pdf_dict_put(ctx, second, PDF_NAME(FT), PDF_NAME(Sig));
        pdf_dict_put_text_string(ctx, second, PDF_NAME(T), "Broken2");
        pdf_dict_put(ctx, second, PDF_NAME(V), value);
        pdf_drop_obj(ctx, value);
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
        pdf_array_insert(ctx, pdf_dict_get(ctx, form, PDF_NAME(Fields)), cycle.get(), 0);
        pdf_save_document(ctx, fixture.get(), cyclicName.constData(), &pdf_default_write_options);
    });
    auto deep = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
        return pdf_add_new_dict(ctx, fixture.get(), 1);
    }), pdf_drop_obj);
    mupdf::call(ctx, [&]() noexcept {
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, fixture.get()), PDF_NAME(Root));
        pdf_obj *fields = pdf_dict_getp(ctx, root, "AcroForm/Fields");
        pdf_array_delete(ctx, fields, 0); // Remove the cycle before preparing other bad branches.
        pdf_obj *badKids = pdf_new_dict(ctx, fixture.get(), 1);
        pdf_dict_put_int(ctx, badKids, PDF_NAME(Kids), 7);
        pdf_array_insert_drop(ctx, fields, badKids, 0);
        pdf_save_document(ctx, fixture.get(), invalidKidsName.constData(), &pdf_default_write_options);
        pdf_array_delete(ctx, fields, 0);
        pdf_array_insert_drop(ctx, fields, pdf_new_int(ctx, 7), 0);
        pdf_save_document(ctx, fixture.get(), invalidFieldName.constData(), &pdf_default_write_options);
        pdf_array_delete(ctx, fields, 0);
        pdf_obj *parent = deep.get();
        for (int depth = 0; depth < 103; ++depth) {
            pdf_obj *kids = pdf_dict_put_array(ctx, parent, PDF_NAME(Kids), 1);
            pdf_array_push_drop(ctx, kids, pdf_add_new_dict(ctx, fixture.get(), 1));
            parent = pdf_array_get(ctx, kids, 0);
        }
        pdf_dict_put(ctx, parent, PDF_NAME(FT), PDF_NAME(Sig));
        pdf_array_insert(ctx, fields, deep.get(), 0);
        pdf_save_document(ctx, fixture.get(), deepName.constData(), &pdf_default_write_options);
        pdf_obj *form = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
        pdf_dict_put_int(ctx, form, PDF_NAME(Fields), 7);
        pdf_save_document(ctx, fixture.get(), invalidFormName.constData(), &pdf_default_write_options);
    });
    CountingVerifier counting;
    const auto badTime = jpdf::crypto::ValidationTime::atSystemTime(
        jpdf::crypto::detail::validationClockBounds().build);
    const auto blocked = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting, {}, badTime);
    if (blocked.size() != 3 || counting.creations != 1 || counting.drops != 1
        || counting.digestCalls != 2 || counting.certificateCalls != 0
        || !counting.metadataTimes.isEmpty()) return 21;
    for (int i = 0; i < 2; ++i) {
        if (blocked[i].integrity != DigitalSignatureStatus::Integrity::Intact || !blocked[i].error.isEmpty()
            || blocked[i].clockError.isEmpty() || blocked[i].certificateTrusted
            || blocked[i].certificateValidation != CertificateChainValidation::NotChecked
            || blocked[i].timestamp.status != SignatureTimestamp::Status::NotChecked
            || !blocked[i].signerChain.isEmpty()) return 22;
    }
    if (blocked[2].signedField || !blocked[2].clockError.isEmpty()) return 24;
    counting.digestResult = PDF_SIGNATURE_ERROR_DIGEST_FAILURE;
    const auto altered = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting, {}, badTime);
    if (altered[0].integrity != DigitalSignatureStatus::Integrity::Invalid
        || altered[0].clockError.isEmpty() || altered[0].certificateTrusted
        || counting.certificateCalls != 0 || !counting.metadataTimes.isEmpty()) return 25;
    counting.digestResult = PDF_SIGNATURE_ERROR_OKAY;
    const auto operationTime = jpdf::crypto::ValidationTime::atSystemTime(QDateTime::currentDateTimeUtc());
    for (int iteration = 0; iteration < 10; ++iteration) {
        const auto cyclic = verifySavedPdf(ctx, dir.filePath("cyclic.pdf"), counting);
        if (cyclic.size() != 4 || !cyclic[0].error.contains("recursive signature field hierarchy")
            || cyclic[0].certificateTrusted || cyclic[0].digestValid
            || cyclic[1].fieldName != "Broken" || cyclic[2].fieldName != "Broken2"
            || cyclic[3].signedField || !cyclic[3].error.isEmpty()) return 17;
        const auto statuses = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting, {}, operationTime);
        if (statuses.size() != 3 || statuses[0].fieldName != "Broken"
            || !statuses[0].signedField || !statuses[0].error.contains("Injected verification failure")
            || statuses[1].fieldName != "Broken2" || !statuses[1].signedField
            || !statuses[1].error.contains("Injected verification failure")
            || statuses[2].fieldName != "Unsigned" || statuses[2].signedField
            || !statuses[2].error.isEmpty()) {
            for (const auto &status : statuses)
                std::cerr << status.fieldName.toStdString() << ": " << status.error.toStdString() << '\n';
            return 18;
        }
        if (counting.drops != 2 + 2 * (iteration + 1)) return 19;
        if (statuses[0].integrity != DigitalSignatureStatus::Integrity::Intact) return 26;
        if (counting.metadataTimes.size() != 4 * (iteration + 1)
            || counting.metadataTimes.last() != operationTime.systemUtc()
            || counting.metadataTimes[counting.metadataTimes.size() - 2] != operationTime.systemUtc()
            || statuses[0].verificationTime != operationTime.systemUtc()
            || statuses[1].verificationTime != operationTime.systemUtc()
            || statuses[2].verificationTime != operationTime.systemUtc()) return 23;
        if (mupdf::call(ctx, []() noexcept { return 42; }) != 42) return 20;
    }
    for (const auto *file : {"invalid-kids.pdf", "invalid-field.pdf", "deep.pdf"}) {
        const auto malformed = verifySavedPdf(ctx, dir.filePath(QString::fromLatin1(file)), counting);
        if (malformed.size() != 4 || malformed[0].error.isEmpty() || malformed[0].digestValid
            || malformed[0].certificateTrusted || malformed[1].fieldName != "Broken"
            || malformed[2].fieldName != "Broken2" || malformed[3].signedField
            || !malformed[3].error.isEmpty()) return 33;
    }
    if (!failsWith([&] { verifySavedPdf(ctx, dir.filePath("invalid-form.pdf"), counting); },
                   "Invalid signature Fields array")) return 34;
    counting.failIdentity = false;
    counting.failCertificate = false;
    for (const auto error : {PDF_SIGNATURE_ERROR_UNKNOWN, PDF_SIGNATURE_ERROR_NO_SIGNATURES,
                            PDF_SIGNATURE_ERROR_NO_CERTIFICATE, PDF_SIGNATURE_ERROR_NOT_SIGNED}) {
        counting.digestResult = error;
        const auto failed = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
        if (failed.size() != 3 || failed[0].digestValid || failed[0].certificateTrusted
            || failed[0].integrity != DigitalSignatureStatus::Integrity::Failed
            || failed[0].certificateValidation != CertificateChainValidation::Failed
            || failed[2].signedField || !failed[2].error.isEmpty()) return 27;
    }
    counting.failDigest = true;
    const auto digestException = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
    if (digestException.size() != 3 || !digestException[0].error.contains("Injected digest failure")
        || digestException[0].integrity != DigitalSignatureStatus::Integrity::Failed
        || digestException[0].digestValid || digestException[0].certificateTrusted) return 28;
    counting.failDigest = false;
    counting.digestResult = PDF_SIGNATURE_ERROR_OKAY;
    counting.failMetadata = true;
    const auto metadataException = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
    if (metadataException.size() != 3 || !metadataException[0].error.contains("Injected metadata failure")
        || metadataException[0].integrity != DigitalSignatureStatus::Integrity::Intact
        || metadataException[0].certificateTrusted) return 29;
    counting.failMetadata = false;
    counting.failCertificate = true;
    const auto certificateException = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
    if (certificateException.size() != 3 || !certificateException[0].error.contains("Injected certificate failure")
        || certificateException[0].integrity != DigitalSignatureStatus::Integrity::Intact
        || certificateException[0].certificateValidation != CertificateChainValidation::Failed
        || certificateException[0].certificateTrusted) return 30;
    counting.failCertificate = false;
    counting.certificateResult = PDF_SIGNATURE_ERROR_OKAY;
    const auto recovered = verifySavedPdf(ctx, dir.filePath("fields.pdf"), counting);
    if (recovered.size() != 3 || !recovered[0].error.isEmpty() || !recovered[0].digestValid
        || !recovered[0].certificateTrusted || recovered[1].fieldName != "Broken2") return 31;
    if (counting.creations != counting.drops) return 32;
    return 0;
}
