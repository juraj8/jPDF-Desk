#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// Failed means validation could not establish trust (e.g. expiry or backend error),
// rather than an explicit rejection of the certificate's trust anchor.
enum class CertificateChainValidation { NotChecked, Accepted, Untrusted, Failed };

struct ValidatedCertificate {
    QByteArray der;
    QString subject;
};
// Leaf first, trusted anchor last. Export only after successful path validation.
using ValidatedCertificateChain = QVector<ValidatedCertificate>;
