#include "jpdf_desk/revocation/revocation.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"

#include <QCoreApplication>
#include <openssl/ocsp.h>
#include <openssl/x509v3.h>
#include <ctime>
#include <iostream>
#include <memory>

using namespace jpdf::revocation;
namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
Key makeKey()
{
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY *key = nullptr;
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) != 1
        || EVP_PKEY_keygen(ctx.get(), &key) != 1) return Key(nullptr, EVP_PKEY_free);
    return Key(key, EVP_PKEY_free);
}
bool extension(X509 *cert, X509 *issuer, int nid, const char *value)
{
    X509V3_CTX context;
    X509V3_set_ctx(&context, issuer, cert, nullptr, nullptr, 0);
    std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> ext(
        X509V3_EXT_conf_nid(nullptr, &context, nid, value), X509_EXTENSION_free);
    return ext && X509_add_ext(cert, ext.get(), -1) == 1;
}
Cert makeCert(EVP_PKEY *key, X509 *issuer, EVP_PKEY *issuerKey, long serial, bool endpoints = true)
{
    Cert cert(X509_new(), X509_free);
    if (!cert) return cert;
    X509_set_version(cert.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), serial);
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 86400);
    X509_set_pubkey(cert.get(), key);
    auto *name = X509_get_subject_name(cert.get());
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char *>(issuer ? "Leaf" : "Issuer"), -1, -1, 0);
    X509_set_issuer_name(cert.get(), issuer ? X509_get_subject_name(issuer) : name);
    if (!extension(cert.get(), issuer ? issuer : cert.get(), NID_basic_constraints,
                   issuer ? "critical,CA:FALSE" : "critical,CA:TRUE")
        || !extension(cert.get(), issuer ? issuer : cert.get(), NID_key_usage,
                      issuer ? "critical,digitalSignature" : "critical,digitalSignature,keyCertSign,cRLSign")) return Cert(nullptr, X509_free);
    if (issuer && endpoints && (!extension(cert.get(), issuer, NID_info_access, "OCSP;URI:http://ocsp.example.test")
        || !extension(cert.get(), issuer, NID_crl_distribution_points, "URI:http://crl.example.test/list.crl"))) return Cert(nullptr, X509_free);
    if (X509_sign(cert.get(), issuer ? issuerKey : key, EVP_sha256()) <= 0) return Cert(nullptr, X509_free);
    return cert;
}
QByteArray encodeCert(X509 *cert)
{
    QByteArray bytes(i2d_X509(cert, nullptr), '\0');
    auto *data = reinterpret_cast<unsigned char *>(bytes.data());
    i2d_X509(cert, &data);
    return bytes;
}
QByteArray ocsp(X509 *cert, X509 *issuer, EVP_PKEY *key, int status, long next = 3600,
                X509 *responder = nullptr, EVP_PKEY *responderKey = nullptr)
{
    std::unique_ptr<OCSP_BASICRESP, decltype(&OCSP_BASICRESP_free)> basic(OCSP_BASICRESP_new(), OCSP_BASICRESP_free);
    std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)> id(OCSP_cert_to_id(EVP_sha1(), cert, issuer), OCSP_CERTID_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> start(ASN1_TIME_adj(nullptr, time(nullptr), 0, -60), ASN1_TIME_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> end(ASN1_TIME_adj(nullptr, time(nullptr), 0, next), ASN1_TIME_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> revoked(ASN1_TIME_adj(nullptr, time(nullptr), 0, -120), ASN1_TIME_free);
    if (!basic || !id || !OCSP_basic_add1_status(basic.get(), id.get(), status, OCSP_REVOKED_STATUS_KEYCOMPROMISE,
            status == V_OCSP_CERTSTATUS_REVOKED ? revoked.get() : nullptr, start.get(), end.get())
        || OCSP_basic_sign(basic.get(), responder ? responder : issuer,
                           responderKey ? responderKey : key, EVP_sha256(), nullptr, 0) != 1) return {};
    std::unique_ptr<OCSP_RESPONSE, decltype(&OCSP_RESPONSE_free)> response(
        OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, basic.get()), OCSP_RESPONSE_free);
    if (!response) return {};
    QByteArray bytes(i2d_OCSP_RESPONSE(response.get(), nullptr), '\0');
    auto *data = reinterpret_cast<unsigned char *>(bytes.data());
    i2d_OCSP_RESPONSE(response.get(), &data);
    return bytes;
}
QByteArray crl(X509 *cert, X509 *issuer, EVP_PKEY *key, bool revoked, long next = 3600,
               bool delta = false, int reason = -1)
{
    std::unique_ptr<X509_CRL, decltype(&X509_CRL_free)> list(X509_CRL_new(), X509_CRL_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> start(ASN1_TIME_adj(nullptr, time(nullptr), 0, -3600), ASN1_TIME_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> end(ASN1_TIME_adj(nullptr, time(nullptr), 0, next), ASN1_TIME_free);
    if (!list) return {};
    X509_CRL_set_version(list.get(), 1);
    X509_CRL_set_issuer_name(list.get(), X509_get_subject_name(issuer));
    X509_CRL_set1_lastUpdate(list.get(), start.get());
    X509_CRL_set1_nextUpdate(list.get(), end.get());
    if (revoked) {
        auto *entry = X509_REVOKED_new();
        X509_REVOKED_set_serialNumber(entry, X509_get_serialNumber(cert));
        X509_REVOKED_set_revocationDate(entry, start.get());
        if (reason >= 0) {
            auto *code = ASN1_ENUMERATED_new();
            ASN1_ENUMERATED_set(code, reason);
            X509_REVOKED_add1_ext_i2d(entry, NID_crl_reason, code, 0, 0);
            ASN1_ENUMERATED_free(code);
        }
        X509_CRL_add0_revoked(list.get(), entry);
    }
    if (delta) {
        auto *number = ASN1_INTEGER_new();
        ASN1_INTEGER_set(number, 1);
        X509_CRL_add1_ext_i2d(list.get(), NID_delta_crl, number, 1, 0);
        ASN1_INTEGER_free(number);
    }
    X509_CRL_sort(list.get());
    if (X509_CRL_sign(list.get(), key, EVP_sha256()) <= 0) return {};
    QByteArray bytes(i2d_X509_CRL(list.get(), nullptr), '\0');
    auto *data = reinterpret_cast<unsigned char *>(bytes.data());
    i2d_X509_CRL(list.get(), &data);
    return bytes;
}
class FakeTransport : public Transport {
public:
    Response ocspReply, crlReply;
    int calls = 0;
    Response fetch(const QUrl &, const QByteArray &request, int timeout, int maximum,
                   const Cancellation &cancelled) override
    {
        if (cancelled && cancelled()) return {{}, "Cancelled"};
        if (timeout <= 0 || maximum <= 0) return {{}, "Invalid limits"};
        ++calls;
        return request.isEmpty() ? crlReply : ocspReply;
    }
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto issuerKey = makeKey(), leafKey = makeKey();
    if (!issuerKey || !leafKey) return 1;
    auto issuer = makeCert(issuerKey.get(), nullptr, nullptr, 1);
    if (!issuer) return 2;
    auto leaf = makeCert(leafKey.get(), issuer.get(), issuerKey.get(), 2);
    auto other = makeCert(leafKey.get(), issuer.get(), issuerKey.get(), 3);
    if (!issuer || !leaf || !other) return 2;
    const auto cert = encodeCert(leaf.get()), authority = encodeCert(issuer.get());
    const auto good = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_GOOD);
    const auto revoked = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_REVOKED);
    const auto unknown = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_UNKNOWN);
    const auto cleanCrl = crl(leaf.get(), issuer.get(), issuerKey.get(), false);
    if (good.isEmpty() || cleanCrl.isEmpty()) return 3;
    if (verifyOcsp(good, cert, authority).status != Status::Good) return 4;
    const auto revokedResult = verifyOcsp(revoked, cert, authority);
    if (revokedResult.status != Status::Revoked || !revokedResult.revokedAt.isValid()
        || revokedResult.revocationReason != OCSP_REVOKED_STATUS_KEYCOMPROMISE) return 5;
    if (verifyOcsp(unknown, cert, authority).status != Status::Unknown) return 6;
    if (verifyOcsp(good, encodeCert(other.get()), authority).status != Status::CheckFailed) return 7;
    if (verifyOcsp(good, cert, cert).status != Status::CheckFailed) return 8;
    QByteArray corrupt = good;
    corrupt[corrupt.size() - 1] = static_cast<char>(corrupt.back() ^ 1);
    if (verifyOcsp(corrupt, cert, authority).status != Status::CheckFailed) return 9;
    if (verifyOcsp(ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_GOOD, -600), cert, authority).status != Status::CheckFailed) return 10;
    if (verifyCrl(cleanCrl, cert, authority).status != Status::Good) return 11;
    if (verifyCrl(crl(leaf.get(), issuer.get(), issuerKey.get(), true), cert, authority).status != Status::Revoked) return 12;
    if (verifyCrl(crl(leaf.get(), issuer.get(), issuerKey.get(), false, -600), cert, authority).status != Status::CheckFailed) return 13;
    if (verifyCrl(crl(leaf.get(), issuer.get(), issuerKey.get(), false, 3600, true), cert, authority).status != Status::CheckFailed) return 14;
    corrupt = cleanCrl;
    corrupt[corrupt.size() - 1] = static_cast<char>(corrupt.back() ^ 1);
    if (verifyCrl(corrupt, cert, authority).status != Status::CheckFailed) return 15;
    Policy small;
    small.maximumResponseBytes = 8;
    if (verifyOcsp(good, cert, authority, small).status != Status::CheckFailed) return 16;
    auto transport = std::make_shared<FakeTransport>();
    transport->ocspReply = {good, {}};
    Checker checker(transport);
    const auto operationTime = jpdf::crypto::ValidationTime{};
    const auto badTime = jpdf::crypto::ValidationTime::atSystemTime(
        jpdf::crypto::detail::validationClockBounds().build);
    const auto blocked = checker.check(cert, authority, badTime);
    if (blocked.status != Status::CheckFailed || !blocked.clockError || transport->calls != 0) return 33;
    const auto blockedOcsp = verifyOcsp(good, cert, authority, {}, badTime);
    const auto blockedCrl = verifyCrl(cleanCrl, cert, authority, {}, badTime);
    if (blockedOcsp.status != Status::CheckFailed || !blockedOcsp.clockError
        || blockedCrl.status != Status::CheckFailed || !blockedCrl.clockError) return 34;
    if (checker.check(cert, authority, operationTime).status != Status::Good || transport->calls != 1) return 17;
    const auto blockedCache = checker.check(cert, authority, badTime);
    if (blockedCache.status != Status::CheckFailed || !blockedCache.clockError || transport->calls != 1) return 35;
    if (checker.check(cert, authority, operationTime).status != Status::Good || transport->calls != 1) return 18;
    const auto staleTime = jpdf::crypto::ValidationTime::atSystemTime(operationTime.systemUtc().addDays(2));
    if (verifyOcsp(good, cert, authority, {}, staleTime).status != Status::CheckFailed
        || verifyCrl(cleanCrl, cert, authority, {}, staleTime).status != Status::CheckFailed) return 36;
    checker.clearCache();
    transport->ocspReply = {{}, "Timeout"};
    transport->crlReply = {cleanCrl, {}};
    const auto fallback = checker.check(cert, authority);
    if (fallback.status != Status::Good || fallback.source != Source::Crl || !fallback.diagnostics.contains("Timeout")) return 19;
    checker.clearCache();
    transport->ocspReply = {unknown, {}};
    transport->crlReply = {{}, "Offline"};
    if (checker.check(cert, authority).status != Status::Unknown) return 20;
    checker.clearCache();
    transport->ocspReply = {{}, "Offline"};
    if (checker.check(cert, authority).status != Status::CheckFailed) return 21;
    auto noEndpoints = makeCert(leafKey.get(), issuer.get(), issuerKey.get(), 4, false);
    if (checker.check(encodeCert(noEndpoints.get()), authority).status != Status::NotChecked) return 22;
    if (checker.check("malformed", authority).status != Status::CheckFailed) return 23;
    if (verifyCrl(crl(leaf.get(), issuer.get(), issuerKey.get(), true, 3600, false, CRL_REASON_REMOVE_FROM_CRL),
                  cert, authority).status != Status::CheckFailed) return 24;
    const auto unauthorized = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_GOOD,
                                   3600, leaf.get(), leafKey.get());
    if (verifyOcsp(unauthorized, cert, authority).status != Status::CheckFailed) return 25;
    Policy noCache;
    noCache.cacheLifetimeSeconds = 0;
    auto uncachedTransport = std::make_shared<FakeTransport>();
    uncachedTransport->ocspReply = {good, {}};
    Checker uncached(uncachedTransport, noCache);
    if (uncached.check(cert, authority).status != Status::Good
        || uncached.check(cert, authority).status != Status::Good || uncachedTransport->calls != 2) return 26;
    const auto keyCompromise = verifyCrl(crl(leaf.get(), issuer.get(), issuerKey.get(), true, 3600, false,
                                          CRL_REASON_KEY_COMPROMISE), cert, authority);
    if (keyCompromise.status != Status::Revoked || keyCompromise.revocationReason != CRL_REASON_KEY_COMPROMISE) return 27;
    auto responder = makeCert(leafKey.get(), issuer.get(), issuerKey.get(), 5, false);
    if (!responder || !extension(responder.get(), issuer.get(), NID_ext_key_usage, "critical,OCSPSigning")
        || X509_sign(responder.get(), issuerKey.get(), EVP_sha256()) <= 0) return 28;
    const auto delegated = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_GOOD,
                               3600, responder.get(), leafKey.get());
    if (verifyOcsp(delegated, cert, authority).status != Status::CheckFailed) return 29;
    auto *nullValue = ASN1_OCTET_STRING_new();
    const unsigned char nullDer[] = {0x05, 0x00};
    ASN1_OCTET_STRING_set(nullValue, nullDer, sizeof nullDer);
    auto *noCheck = X509_EXTENSION_create_by_NID(nullptr, NID_id_pkix_OCSP_noCheck, 0, nullValue);
    const bool added = noCheck && X509_add_ext(responder.get(), noCheck, -1) == 1;
    X509_EXTENSION_free(noCheck);
    ASN1_OCTET_STRING_free(nullValue);
    if (!added || X509_sign(responder.get(), issuerKey.get(), EVP_sha256()) <= 0) return 30;
    const auto delegatedNoCheck = ocsp(leaf.get(), issuer.get(), issuerKey.get(), V_OCSP_CERTSTATUS_GOOD,
                                      3600, responder.get(), leafKey.get());
    if (verifyOcsp(delegatedNoCheck, cert, authority).status != Status::Good) return 31;
    Policy cancelled;
    cancelled.cancelled = [] { return true; };
    const int beforeCancellation = transport->calls;
    Checker stopped(transport, cancelled);
    if (stopped.check(cert, authority).status != Status::CheckFailed
        || transport->calls != beforeCancellation) return 32;
    return 0;
}
