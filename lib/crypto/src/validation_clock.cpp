#include "jpdf_desk/crypto/detail/validation_clock.h"
#include "jpdf_desk/crypto/validation_time.h"
#include "validation_clock_build.h"

#include <QObject>

namespace jpdf::crypto::detail {
ValidationClockBounds validationClockBounds()
{
    return {QDateTime::fromString(QStringLiteral(JPDF_EMBEDDED_TIMESTAMP), Qt::ISODate),
            QDateTime::fromString(QStringLiteral(JPDF_BUILD_TIMESTAMP), Qt::ISODate)};
}

QString validationClockError(const QDateTime &systemTime, const ValidationClockBounds &bounds)
{
    if (!bounds.embedded.isValid() || !bounds.build.isValid() || bounds.build <= bounds.embedded)
        return QObject::tr("Time-dependent validation failed: invalid embedded/build timestamp bounds.");
    if (!systemTime.isValid() || systemTime <= bounds.embedded || systemTime <= bounds.build)
        return QObject::tr("Time-dependent validation failed: system UTC time (%1) must be later than "
                           "the embedded timestamp (%2) and build timestamp (%3). Check the system clock.")
            .arg(systemTime.toUTC().toString(Qt::ISODate), bounds.embedded.toUTC().toString(Qt::ISODate),
                 bounds.build.toUTC().toString(Qt::ISODate));
    return {};
}

} // namespace jpdf::crypto::detail

namespace jpdf::crypto {
ValidationTime::ValidationTime()
    : systemUtc_(QDateTime::currentDateTimeUtc()), evaluationUtc_(systemUtc_) {}

ValidationTime::ValidationTime(const QDateTime &evaluationTime)
    : ValidationTime(QDateTime::currentDateTimeUtc(), evaluationTime) {}

ValidationTime::ValidationTime(const QDateTime &systemTime, const QDateTime &evaluationTime)
    : systemUtc_(systemTime.toUTC()), evaluationUtc_(evaluationTime.toUTC()) {}

ValidationTime ValidationTime::atSystemTime(const QDateTime &systemTime)
{
    return ValidationTime(systemTime, systemTime);
}

QString ValidationTime::clockError() const
{
    return detail::validationClockError(systemUtc_, detail::validationClockBounds());
}

void ValidationTime::requireValidClock() const
{
    const auto error = clockError();
    if (!error.isEmpty()) throw ClockValidationError(error);
}
} // namespace jpdf::crypto
