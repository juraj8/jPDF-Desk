#pragma once

#include <QDateTime>
#include <QString>

namespace jpdf::crypto::detail {
struct ValidationClockBounds {
    QDateTime embedded;
    QDateTime build;
};

ValidationClockBounds validationClockBounds();
// Empty means acceptable. Explicit inputs make boundary checks deterministic in tests.
QString validationClockError(const QDateTime &systemTime, const ValidationClockBounds &bounds);
} // namespace jpdf::crypto::detail
