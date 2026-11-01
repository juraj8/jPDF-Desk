#include "jpdf_desk/signing/pdf_signatures.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QFile>
#include <QObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <ctime>
#include <cstring>
#include <stdexcept>

namespace {
DigitalSignatureStatus checkSignature(fz_context *ctx, pdf_document *doc,
                                      pdf_obj *field, pdf_pkcs7_verifier *verifier,
                                      const VerificationProvider &provider,
                                      const jpdf::crypto::ValidationTime &time)
{
    DigitalSignatureStatus result;
    result.verificationTime = time.systemUtc();
    try {
        auto name = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_load_field_name(ctx, field);
        }), fz_free);
        result.fieldName = QString::fromUtf8(name.get());
        result.signedField = mupdf::call(ctx, [&]() noexcept {
            return pdf_signature_is_signed(ctx, doc, field);
        });
        if (result.signedField) {
            result.clockError = time.clockError();
            const bool documentTimestamp = mupdf::call(ctx, [&]() noexcept {
                pdf_obj *value = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
                return std::strcmp(pdf_to_name(ctx, pdf_dict_get(ctx, value, PDF_NAME(SubFilter))), "ETSI.RFC3161") == 0;
            });
            if (documentTimestamp) {
                result.error = QStringLiteral("Separate PDF document timestamps are not supported; only embedded signature timestamps are checked.");
                return result;
            }
            const auto signingTime = mupdf::call(ctx, [&]() noexcept {
                pdf_obj *value = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
                return pdf_dict_get_date(ctx, value, PDF_NAME(M));
            });
            // MuPDF returns zero for missing or malformed dates.
            if (signingTime != 0)
                result.signingTime = QDateTime::fromSecsSinceEpoch(signingTime, QTimeZone::UTC);
            // Integrity is a cryptographic check, independent of clock/trust policy.
            result.integrity = DigitalSignatureStatus::Integrity::Failed;
            const auto digest = mupdf::call(ctx, [&]() noexcept {
                return pdf_check_digest(ctx, verifier, doc, field);
            });
            result.digestValid = digest == PDF_SIGNATURE_ERROR_OKAY;
            result.integrity = result.digestValid ? DigitalSignatureStatus::Integrity::Intact
                : digest == PDF_SIGNATURE_ERROR_DIGEST_FAILURE ? DigitalSignatureStatus::Integrity::Invalid
                : DigitalSignatureStatus::Integrity::Failed;
            result.digestStatus = QString::fromUtf8(pdf_signature_error_description(digest));
            result.changedSinceSigning = mupdf::call(ctx, [&]() noexcept {
                return pdf_signature_incremental_change_since_signing(ctx, doc, field) != 0;
            });
            result.coverageChecked = true;
            // Never invoke either chain policy or metadata/timestamp validation with
            // an unacceptable clock, even if a provider would otherwise accept it.
            if (!result.clockError.isEmpty()) return result;
            char *contents = nullptr;
            const auto length = mupdf::call(ctx, [&]() noexcept {
                return pdf_signature_contents(ctx, doc, field, &contents);
            });
            auto ownedContents = mupdf::own(ctx, contents, fz_free);
            const auto metadata = provider.certificateDetails(QByteArray(contents, length), time);
            result.certificateValidFrom = metadata.validFrom;
            result.certificateValidUntil = metadata.validUntil;
            result.certificateProblems = metadata.problems;
            result.certificateDescription = metadata.description;
            result.timestamp = metadata.timestamp;
            result.signerChain = metadata.signerChain;
            if (!metadata.error.isEmpty()) {
                result.error = metadata.error;
                result.certificateValidation = DigitalSignatureStatus::CertificateValidation::Failed;
                result.certificateStatus = metadata.error;
                result.signerChain.clear();
                result.timestamp = {SignatureTimestamp::Status::Failed, {}, metadata.error};
                return result;
            }
            auto dn = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                return pdf_signature_get_signatory(ctx, verifier, doc, field);
            }), pdf_signature_drop_distinguished_name);
            if (dn) {
                auto signerName = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                    return pdf_signature_format_distinguished_name(ctx, dn.get());
                }), fz_free);
                result.signer = QString::fromUtf8(signerName.get());
            }
            using Validation = DigitalSignatureStatus::CertificateValidation;
            result.certificateValidation = Validation::Failed;
            const auto certificate = mupdf::call(ctx, [&]() noexcept {
                return pdf_check_certificate(ctx, verifier, doc, field);
            });
            result.certificateTrusted = certificate == PDF_SIGNATURE_ERROR_OKAY;
            switch (certificate) {
            case PDF_SIGNATURE_ERROR_OKAY: result.certificateValidation = Validation::Accepted; break;
            case PDF_SIGNATURE_ERROR_SELF_SIGNED:
            case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
            case PDF_SIGNATURE_ERROR_NOT_TRUSTED: result.certificateValidation = Validation::Untrusted; break;
            default: result.certificateValidation = Validation::Failed; break;
            }
            result.certificateStatus = QString::fromUtf8(pdf_signature_error_description(certificate));
            if (metadata.systemChain != Validation::NotChecked) {
                result.certificateDescription += QStringLiteral("\nMuPDF built-in chain check: %1\nSystem/OpenSSL chain check: %2")
                    .arg(result.certificateStatus, metadata.systemChainDescription);
                if (metadata.systemChain == Validation::Accepted) {
                    result.certificateTrusted = true;
                    result.certificateValidation = Validation::Accepted;
                    result.certificateStatus = QStringLiteral("Accepted by system/OpenSSL CA roots.");
                } else if (result.certificateValidation == Validation::Failed
                           && metadata.systemChain == Validation::Untrusted) {
                    result.certificateValidation = Validation::Untrusted;
                    result.certificateStatus = metadata.systemChainDescription;
                } else if (!result.certificateTrusted && metadata.systemChain == Validation::Failed) {
                    // A date/purpose/backend failure is not proof that the root is untrusted.
                    result.certificateValidation = Validation::Failed;
                }
            }
        }
    } catch (const jpdf::crypto::ClockValidationError &error) {
        result.clockError = QString::fromUtf8(error.what());
    } catch (const std::runtime_error &error) {
        // Preserve per-field MuPDF errors, but let allocation failures unwind.
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

void checkSignatureFields(fz_context *ctx, pdf_document *doc, pdf_obj *field,
                          pdf_pkcs7_verifier *verifier,
                          const VerificationProvider &provider,
                          const jpdf::crypto::ValidationTime &time,
                          QVector<DigitalSignatureStatus> &results,
                          pdf_cycle_list *parent = nullptr, int depth = 0)
{
    try {
        pdf_cycle_list cycle;
        if (!field || depth > 100 || mupdf::call(ctx, [&]() noexcept {
                return !pdf_is_dict(ctx, field) || pdf_cycle(ctx, &cycle, parent, field);
            }))
            throw std::runtime_error("Invalid or recursive signature field hierarchy");
        pdf_obj *kids = mupdf::call(ctx, [&]() noexcept {
            pdf_obj *value = pdf_dict_get(ctx, field, PDF_NAME(Kids));
            if (value && !pdf_is_array(ctx, value))
                fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid signature field Kids array");
            return value;
        });
        const int count = mupdf::call(ctx, [&]() noexcept { return pdf_array_len(ctx, kids); });
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                pdf_obj *child = mupdf::call(ctx, [&]() noexcept { return pdf_array_get(ctx, kids, i); });
                checkSignatureFields(ctx, doc, child, verifier, provider, time, results, &cycle, depth + 1);
            }
        } else if (mupdf::call(ctx, [&]() noexcept {
                return pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) == PDF_NAME(Sig);
            })) {
            results.append(checkSignature(ctx, doc, field, verifier, provider, time));
        }
    } catch (const std::runtime_error &error) {
        // Report only the broken branch; preserve other fields and keep traversing
        // siblings. Do not try to read an inherited name from a recursive field.
        DigitalSignatureStatus result;
        result.fieldName = QObject::tr("Unreadable signature field");
        result.verificationTime = time.systemUtc();
        result.error = QString::fromUtf8(error.what());
        results.append(result);
    }
}
}

void signPdfSnapshot(fz_context *ctx, const QString &input, const QString &output,
                     const SigningProvider &provider, const SigningIdentity &identity,
                     const QString &documentPassword)
{
    QTemporaryDir staging;
    if (!staging.isValid()) throw std::runtime_error("Cannot create temporary signing directory.");
    // MuPDF's filename APIs take UTF-8 on every platform.
    const QByteArray inputName = input.toUtf8();
    const QByteArray password = documentPassword.toUtf8();
    const QString signedPath = staging.filePath("signed.pdf");
    const QByteArray outputName = signedPath.toUtf8();
    // Invoke providers outside fz_try: a C++ exception must not bypass MuPDF's
    // exception-stack cleanup. No PDF resources have been acquired yet.
    {
        auto signer = mupdf::own(ctx, provider.createSigner(ctx, identity), pdf_drop_signer);
        if (!signer) throw std::runtime_error("Signing provider returned no signer.");
        auto doc = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_open_document(ctx, inputName.constData());
        }), pdf_drop_document);
        mupdf::call(ctx, [&]() noexcept {
            if (pdf_needs_password(ctx, doc.get())
                && !pdf_authenticate_password(ctx, doc.get(), password.constData()))
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid PDF password");
        });
        auto page = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_load_page(ctx, doc.get(), 0);
        }), pdf_drop_page);
        mupdf::call(ctx, [&]() noexcept {
            char name[] = "JPDFDeskDigitalSignature";
            pdf_annot *widget = pdf_create_signature_widget(ctx, page.get(), name);
            // Invisible signature: independent of the handwritten image.
            pdf_sign_signature_with_appearance(ctx, widget, signer.get(), std::time(nullptr), nullptr);
            pdf_save_document(ctx, doc.get(), outputName.constData(), &pdf_default_write_options);
        });
    }

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
                                              const VerificationProvider &provider, const QString &documentPassword,
                                              const jpdf::crypto::ValidationTime &time)
{
    QVector<DigitalSignatureStatus> results;
    const QByteArray filename = path.toUtf8();
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
        pdf_obj *fields = pdf_dict_get(ctx, form, PDF_NAME(Fields));
        if (fields && !pdf_is_array(ctx, fields))
            fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid signature Fields array");
        return fields;
    });
    const int count = mupdf::call(ctx, [&]() noexcept { return pdf_array_len(ctx, fields); });
    for (int i = 0; i < count; ++i) {
        pdf_obj *field = mupdf::call(ctx, [&]() noexcept { return pdf_array_get(ctx, fields, i); });
        checkSignatureFields(ctx, saved.get(), field, verifier.get(), provider, time, results);
    }
    return results;
}
