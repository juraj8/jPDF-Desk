#include "jpdf_desk/signing/pdf_signatures.h"

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
    pdf_pkcs7_distinguished_name *dn = nullptr;
    char *name = nullptr;
    fz_var(dn);
    fz_var(name);
    fz_try(ctx) {
        name = pdf_load_field_name(ctx, field);
        result.fieldName = QString::fromUtf8(name);
        fz_free(ctx, name);
        name = nullptr;
        result.signedField = pdf_signature_is_signed(ctx, doc, field);
        if (result.signedField) {
            dn = pdf_signature_get_signatory(ctx, verifier, doc, field);
            if (dn) {
                name = pdf_signature_format_distinguished_name(ctx, dn);
                result.signer = QString::fromUtf8(name);
            }
            const pdf_signature_error digest = pdf_check_digest(ctx, verifier, doc, field);
            result.digestValid = digest == PDF_SIGNATURE_ERROR_OKAY;
            result.digestStatus = QString::fromUtf8(pdf_signature_error_description(digest));
            const pdf_signature_error certificate = pdf_check_certificate(ctx, verifier, doc, field);
            result.certificateTrusted = certificate == PDF_SIGNATURE_ERROR_OKAY;
            result.certificateStatus = QString::fromUtf8(pdf_signature_error_description(certificate));
            result.changedSinceSigning = pdf_signature_incremental_change_since_signing(ctx, doc, field) != 0;
        }
    }
    fz_always(ctx) {
        fz_free(ctx, name);
        pdf_signature_drop_distinguished_name(ctx, dn);
    }
    fz_catch(ctx) { result.error = QString::fromUtf8(fz_caught_message(ctx)); }
    return result;
}

void checkSignatureFields(fz_context *ctx, pdf_document *doc, pdf_obj *field,
                          pdf_pkcs7_verifier *verifier,
                          QVector<DigitalSignatureStatus> &results,
                          pdf_cycle_list *parent = nullptr, int depth = 0)
{
    pdf_cycle_list cycle;
    if (!field || depth > 100 || pdf_cycle(ctx, &cycle, parent, field))
        fz_throw(ctx, FZ_ERROR_SYNTAX, "Invalid or recursive signature field hierarchy");
    pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
    if (pdf_array_len(ctx, kids) > 0) {
        for (int i = 0; i < pdf_array_len(ctx, kids); ++i)
            checkSignatureFields(ctx, doc, pdf_array_get(ctx, kids, i), verifier,
                                 results, &cycle, depth + 1);
    } else if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) == PDF_NAME(Sig)) {
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
    pdf_pkcs7_verifier *verifier = provider.createVerifier(ctx);
    if (!verifier) throw std::runtime_error("Verification provider returned no verifier.");
    pdf_document *saved = nullptr;
    int failed = 0;
    fz_var(saved);
    fz_try(ctx) {
        saved = pdf_open_document(ctx, filename.constData());
        if (pdf_needs_password(ctx, saved) && !pdf_authenticate_password(ctx, saved, password.constData()))
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid PDF password");
        pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, saved), PDF_NAME(Root));
        pdf_obj *form = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
        pdf_obj *fields = pdf_dict_get(ctx, form, PDF_NAME(Fields));
        for (int i = 0; i < pdf_array_len(ctx, fields); ++i)
            checkSignatureFields(ctx, saved, pdf_array_get(ctx, fields, i), verifier, results);
    }
    fz_always(ctx) {
        pdf_drop_verifier(ctx, verifier);
        pdf_drop_document(ctx, saved);
    }
    fz_catch(ctx) { failed = 1; }
    check(ctx, failed);
    return results;
}
