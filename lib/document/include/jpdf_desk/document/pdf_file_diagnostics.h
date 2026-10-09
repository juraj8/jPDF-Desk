#pragma once

#include <QString>

// Bounded, read-only diagnostics for PDFs that cannot be opened normally.
// Reports raw, unauthenticated structural hints, not decrypted document metadata.
QString pdfFileDiagnostics(const QString &path);
