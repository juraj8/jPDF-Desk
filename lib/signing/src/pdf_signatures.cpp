#include "jpdf_desk/signing/pdf_signatures.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <ctime>
#include <stdexcept>

namespace {
void check(fz_context *ctx, bool failed)
{
    if (failed) throw std::runtime_error(fz_caught_message(ctx));
}

DigitalSignatureStatus checkSignature(fz_context *ctx, pdf_document *doc,
                                      pdf_obj *field, pdf_pkcs7_verifier *verifier)
{
    DigitalSignatureStatus result;
    try {
        auto name = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_load_field_name(ctx, field);
        }), fz_free);
        result.fieldName = QString::fromUtf8(name.get());
        result.signedField = mupdf::call(ctx, [&]() noexcept {
            return pdf_signature_is_signed(ctx, doc, field);
        });
        if (result.signedField) {
            auto dn = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                return pdf_signature_get_signatory(ctx, verifier, doc, field);
            }), pdf_signature_drop_distinguished_name);
            if (dn) {
                auto signerName = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                    return pdf_signature_format_distinguished_name(ctx, dn.get());
                }), fz_free);
                result.signer = QString::fromUtf8(signerName.get());
            }
            const auto digest = mupdf::call(ctx, [&]() noexcept {
                return pdf_check_digest(ctx, verifier, doc, field);
            });
            result.digestValid = digest == PDF_SIGNATURE_ERROR_OKAY;
            result.digestStatus = QString::fromUtf8(pdf_signature_error_description(digest));
            const auto certificate = mupdf::call(ctx, [&]() noexcept {
                return pdf_check_certificate(ctx, verifier, doc, field);
            });
            result.certificateTrusted = certificate == PDF_SIGNATURE_ERROR_OKAY;
            result.certificateStatus = QString::fromUtf8(pdf_signature_error_description(certificate));
            result.changedSinceSigning = mupdf::call(ctx, [&]() noexcept {
                return pdf_signature_incremental_change_since_signing(ctx, doc, field) != 0;
            });
        }
    } catch (const std::runtime_error &error) {
        // Preserve per-field MuPDF errors, but let allocation failures unwind.
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

void checkSignatureFields(fz_context *ctx, pdf_document *doc, pdf_obj *field,
                          pdf_pkcs7_verifier *verifier,
                          QVector<DigitalSignatureStatus> &results,
                          pdf_cycle_list *parent = nullptr, int depth = 0)
{
    pdf_cycle_list cycle;
    if (!field || depth > 100 || mupdf::call(ctx, [&]() noexcept {
            return pdf_cycle(ctx, &cycle, parent, field);
        }))
        throw std::runtime_error("Invalid or recursive signature field hierarchy");
    pdf_obj *kids = mupdf::call(ctx, [&]() noexcept {
        return pdf_dict_get(ctx, field, PDF_NAME(Kids));
    });
    const int count = mupdf::call(ctx, [&]() noexcept { return pdf_array_len(ctx, kids); });
    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            pdf_obj *child = mupdf::call(ctx, [&]() noexcept { return pdf_array_get(ctx, kids, i); });
            checkSignatureFields(ctx, doc, child, verifier, results, &cycle, depth + 1);
        }
    } else if (mupdf::call(ctx, [&]() noexcept {
            return pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) == PDF_NAME(Sig);
        })) {
        results.append(checkSignature(ctx, doc, field, verifier));
    }
}
}

void signPdfSnapshot(fz_context *ctx, const QString &input, const QString &output,
                     const SigningProvider &provider, const SigningIdentity &identity,
                     const QString &documentPassword)
{
    QTemporaryDir staging;
    if (!staging.isValid()) throw std::runtime_error("Cannot create temporary signing directory.");
    const QByteArray inputName = QFile::encodeName(input);
    const QByteArray password = documentPassword.toUtf8();
    const QString signedPath = staging.filePath("signed.pdf");
    const QByteArray outputName = QFile::encodeName(signedPath);
    // Invoke providers outside fz_try: a C++ exception must not bypass MuPDF's
    // exception-stack cleanup. No PDF resources have been acquired yet.
    pdf_pkcs7_signer *signer = provider.createSigner(ctx, identity);
    if (!signer) throw std::runtime_error("Signing provider returned no signer.");
    pdf_document *doc = nullptr;
    pdf_page *page = nullptr;
    int failed = 0;
    fz_var(doc);
    fz_var(page);
    fz_try(ctx) {
        doc = pdf_open_document(ctx, inputName.constData());
        if (pdf_needs_password(ctx, doc) && !pdf_authenticate_password(ctx, doc, password.constData()))
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid PDF password");
        page = pdf_load_page(ctx, doc, 0);
        char name[] = "JPDFDeskDigitalSignature";
        pdf_annot *widget = pdf_create_signature_widget(ctx, page, name);
        // Invisible signature: independent of the handwritten image.
        pdf_sign_signature_with_appearance(ctx, widget, signer, std::time(nullptr), nullptr);
        pdf_save_document(ctx, doc, outputName.constData(), &pdf_default_write_options);
    }
    fz_always(ctx) {
        pdf_drop_page(ctx, page);
        pdf_drop_document(ctx, doc);
        pdf_drop_signer(ctx, signer);
    }
    fz_catch(ctx) { failed = 1; }
    check(ctx, failed);

    // Never publish a partly signed file or replace an existing output on failure.
    QFile source(signedPath);
    QSaveFile destination(output);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot open signed PDF output.");
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() || destination.write(chunk) != chunk.size())
            throw std::runtime_error("Cannot copy signed PDF output.");
    }
    if (!destination.commit()) throw std::runtime_error("Cannot commit signed PDF output.");
}

QVector<DigitalSignatureStatus> verifySavedPdf(fz_context *ctx, const QString &path,
                                              const VerificationProvider &provider, const QString &documentPassword)
{
    QVector<DigitalSignatureStatus> results;
    const QByteArray filename = QFile::encodeName(path);
    const QByteArray password = documentPassword.toUtf8();
    // Providers and all Qt allocations run outside MuPDF longjmp boundaries.
    auto verifier = mupdf::own(ctx, provider.createVerifier(ctx), pdf_drop_verifier);
    if (!verifier) throw std::runtime_error("Verification provider returned no verifier.");
    auto saved = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
        return pdf_open_document(ctx, filename.constData());
    }), pdf_drop_document);
    pdf_obj *fields = mupdf::call(ctx, [&]() noexcept {
        if (pdf_needs_password(ctx, saved.get()) &&
            !pdf_authenticate_password(ctx, saved.get(), password.constData()))
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid PDF password");
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, saved.get()), PDF_NAME(Root));
        pdf_obj *form = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
        return pdf_dict_get(ctx, form, PDF_NAME(Fields));
    });
    const int count = mupdf::call(ctx, [&]() noexcept { return pdf_array_len(ctx, fields); });
    for (int i = 0; i < count; ++i) {
        pdf_obj *field = mupdf::call(ctx, [&]() noexcept { return pdf_array_get(ctx, fields, i); });
        checkSignatureFields(ctx, saved.get(), field, verifier.get(), results);
    }
    return results;
}
