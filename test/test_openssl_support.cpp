#include "jpdf_desk/crypto/detail/openssl_support.h"

#include <QCoreApplication>
#include <iostream>
#include <memory>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace jpdf::crypto::detail;
    const auto require = [](bool condition, const char *message) {
        if (!condition) std::cerr << message << '\n';
        return condition;
    };
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> time(ASN1_TIME_new(), ASN1_TIME_free);
    if (!require(bool(time), "Cannot allocate ASN.1 time")) return 1;
    if (!require(!asn1DateTime(nullptr).isValid() && !asn1DateTime(time.get()).isValid(),
                 "Missing and malformed times must remain invalid")) return 1;
    for (const char *encoded : {"240229123456Z", "20240229123456Z"}) {
        if (!require(ASN1_TIME_set_string(time.get(), encoded) == 1, "Cannot set ASN.1 time")) return 1;
        const auto date = asn1DateTime(time.get());
        if (!require(date == QDateTime(QDate(2024, 2, 29), QTime(12, 34, 56), QTimeZone::UTC)
                     && date.offsetFromUtc() == 0, "ASN.1 time must convert to UTC")) return 1;
    }
    ERR_clear_error();
    if (!require(takeOpenSslErrors().isEmpty(), "Empty error queue must yield no diagnostics")) return 1;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    ERR_raise(ERR_LIB_ASN1, ERR_R_INTERNAL_ERROR);
    ERR_raise(ERR_LIB_X509, ERR_R_INTERNAL_ERROR);
#else
    ERR_put_error(ERR_LIB_ASN1, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);
    ERR_put_error(ERR_LIB_X509, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);
#endif
    if (!require(takeOpenSslErrors().size() == 2 && ERR_peek_error() == 0,
                 "Diagnostics must drain all queued errors")) return 1;
    return 0;
}
