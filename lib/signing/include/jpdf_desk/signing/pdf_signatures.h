#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include "jpdf_desk/signing/signature_services.h"

struct pdf_document;

// MuPDF integration boundary: providers supply crypto handles, while this
// library owns signature embedding, saved-file verification, and publication.
void signPdfSnapshot(fz_context *context, const QString &input, const QString &output,
                     const SigningProvider &provider, const SigningIdentity &identity,
                     const QString &documentPassword = {});
// Integrity is checked independently. An unacceptable clock is reported per signed
// field in clockError; certificate trust and timestamps remain NotChecked.
QVector<DigitalSignatureStatus> verifySavedPdf(fz_context *context, const QString &path,
                                              const VerificationProvider &provider,
                                              const QString &documentPassword = {},
                                              const jpdf::crypto::ValidationTime &time = {});
// Verify a freshly opened, authenticated saved revision without reopening its path.
QVector<DigitalSignatureStatus> verifyOpenedPdf(fz_context *context, pdf_document *document,
                                               const VerificationProvider &provider,
                                               const jpdf::crypto::ValidationTime &time = {});
