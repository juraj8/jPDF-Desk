#pragma once

#include <QDateTime>
#include <QString>
#include <stdexcept>

namespace jpdf::crypto {
class ClockValidationError final : public std::runtime_error {
public:
    explicit ClockValidationError(const QString &message) : std::runtime_error(message.toStdString()) {}
};

// An operation-scoped snapshot. Default construction reads the system clock once.
// An explicit evaluation date supports offline historical evidence checks while
// keeping the clock sanity check tied to the captured system time.
class ValidationTime {
public:
    ValidationTime();
    ValidationTime(const QDateTime &evaluationTime);
    // Clock injection for callers/tests; never a persisted preference or UI override.
    static ValidationTime atSystemTime(const QDateTime &systemTime);
    const QDateTime &systemUtc() const { return systemUtc_; }
    const QDateTime &evaluationUtc() const { return evaluationUtc_; }
    QString clockError() const;
    void requireValidClock() const;
private:
    ValidationTime(const QDateTime &systemTime, const QDateTime &evaluationTime);
    QDateTime systemUtc_;
    QDateTime evaluationUtc_;
};
} // namespace jpdf::crypto
