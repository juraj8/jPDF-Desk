#pragma once

#include <QDateTime>
#include <QStringList>
#include <QTimeZone>
#include <openssl/asn1.h>
#include <openssl/err.h>
#include <ctime>

namespace jpdf::crypto::detail {
// ASN.1 times have no local-time interpretation. Invalid/missing values stay invalid.
inline QDateTime asn1DateTime(const ASN1_TIME *value)
{
    std::tm time{};
    if (!value || ASN1_TIME_to_tm(value, &time) != 1) return {};
    return QDateTime(QDate(time.tm_year + 1900, time.tm_mon + 1, time.tm_mday),
                     QTime(time.tm_hour, time.tm_min, time.tm_sec), QTimeZone::UTC);
}

// Drain the calling thread's error queue, leaving policy and message formatting to callers.
inline QStringList takeOpenSslErrors()
{
    QStringList errors;
    for (unsigned long error; (error = ERR_get_error()) != 0;) {
        char text[256];
        ERR_error_string_n(error, text, sizeof text);
        errors << QString::fromLatin1(text);
    }
    return errors;
}
} // namespace jpdf::crypto::detail
