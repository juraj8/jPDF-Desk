#pragma once

#include "certificate_validation.h"
#include <QDateTime>
#include <QString>

struct SignatureTimestamp {
    enum class Status { NotChecked, Absent, Verified, Invalid, Failed };
    Status status = Status::NotChecked;
    QDateTime time; // Trusted only when status is Verified; proves existence by this time.
    QString description;
    ValidatedCertificateChain certificateChain;
};
