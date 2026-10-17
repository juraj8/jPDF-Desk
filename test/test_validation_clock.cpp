#include "jpdf_desk/crypto/detail/validation_clock.h"
#include "jpdf_desk/crypto/validation_time.h"

#include <QCoreApplication>
#include <QTimeZone>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace jpdf::crypto::detail;
    const auto embedded = QDateTime::fromString(QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
    const auto build = embedded.addSecs(60);
    const ValidationClockBounds bounds{embedded, build};
    const auto require = [](bool condition, const char *message) {
        if (!condition) std::cerr << message << '\n';
        return condition;
    };
    for (const auto &now : {QDateTime(), embedded.addSecs(-1), embedded,
                           embedded.addSecs(1), build.addMSecs(-1), build}) {
        const auto error = validationClockError(now, bounds);
        if (!require(!error.isEmpty() && error.contains("system clock"),
                     "Invalid, earlier or equal system time must fail")) return 1;
    }
    if (!require(validationClockError(build.addMSecs(1), bounds).isEmpty(),
                 "Time strictly later than both bounds must pass")) return 2;
    if (!require(validationClockError(build.addSecs(1).toTimeZone(QTimeZone::fromSecondsAheadOfUtc(3600)), bounds).isEmpty(),
                 "Comparisons must use absolute time, not the local timezone")) return 3;
    for (const auto &invalid : {ValidationClockBounds{{}, build}, ValidationClockBounds{embedded, {}},
                               ValidationClockBounds{embedded, embedded},
                               ValidationClockBounds{build, embedded}}) {
        if (!require(!validationClockError(build.addSecs(1), invalid).isEmpty(),
                     "Invalid or unordered captured bounds must fail")) return 4;
    }
    const auto captured = validationClockBounds();
    if (!require(captured.embedded.isValid() && captured.build > captured.embedded,
                 "Generated build timestamp must be later than the fixed source timestamp")) return 5;
    if (!require(validationClockError(captured.build, captured).contains(captured.build.toString(Qt::ISODate)),
                 "Failure diagnostics must include the captured bound")) return 6;
    if (!require(jpdf::crypto::ValidationTime{}.clockError().isEmpty(), "Test host clock must be later than the build")) return 7;
    const auto injected = jpdf::crypto::ValidationTime::atSystemTime(captured.build);
    try {
        injected.requireValidClock();
        return 8;
    } catch (const jpdf::crypto::ClockValidationError &) {}
    const jpdf::crypto::ValidationTime snapshot;
    if (!require(snapshot.systemUtc() == snapshot.evaluationUtc() && snapshot.clockError().isEmpty(),
                 "Default validation must capture a single system time")) return 9;
    const jpdf::crypto::ValidationTime historical(embedded);
    if (!require(historical.evaluationUtc() == embedded && historical.systemUtc() > captured.build
                 && historical.clockError().isEmpty(),
                 "Historical evidence time must not override the captured system time")) return 10;
    return 0;
}
