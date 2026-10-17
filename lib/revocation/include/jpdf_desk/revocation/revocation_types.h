#pragma once

#include <QDateTime>
#include <QStringList>

namespace jpdf::revocation {
enum class Status { NotChecked, Good, Revoked, Unknown, CheckFailed };
enum class Source { None, Ocsp, Crl };

struct Result {
    Status status = Status::NotChecked;
    Source source = Source::None;
    QDateTime thisUpdate;
    QDateTime nextUpdate;
    QDateTime revokedAt;
    QDateTime invalidityDate;
    int revocationReason = -1;
    QStringList diagnostics;
    bool clockError = false; // A clock failure is not evidence of revocation/invalidity.
};
} // namespace jpdf::revocation
