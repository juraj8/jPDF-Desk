#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include "jpdf_desk/signing/signature_services.h"

// MuPDF integration boundary: providers supply crypto handles, while this
// library owns signature embedding, saved-file verification, and publication.
void signPdfSnapshot(fz_context *context, const QString &input, const QString &output,
                     const SigningProvider &provider, const SigningIdentity &identity,
                     const QString &documentPassword = {});
QVector<DigitalSignatureStatus> verifySavedPdf(fz_context *context, const QString &path,
                                              const VerificationProvider &provider,
                                              const QString &documentPassword = {});
