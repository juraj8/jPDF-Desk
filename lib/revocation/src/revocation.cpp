#include "jpdf_desk/revocation/revocation.h"
#include "jpdf_desk/crypto/detail/openssl_support.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHash>
#include <openssl/err.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <utility>

namespace jpdf::revocation {
namespace {
using jpdf::crypto::detail::asn1DateTime;
using Certificate = std::unique_ptr<X509, decltype(&X509_free)>;

Certificate decode(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > 1024 * 1024) return Certificate(nullptr, X509_free);
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    Certificate cert(d2i_X509(nullptr, &data, bytes.size()), X509_free);
    if (data != reinterpret_cast<const unsigned char *>(bytes.constData()) + bytes.size()) cert.reset();
    return cert;
}

Result failed(Source source, const QString &message, bool clockError = false)
{
    Result result;
    result.status = Status::CheckFailed;
    result.source = source;
    result.clockError = clockError;
    result.diagnostics << message;
    result.diagnostics << jpdf::crypto::detail::takeOpenSslErrors();
    return result;
}

bool validPolicy(const Policy &policy)
{
    return policy.requestTimeoutMs > 0 && policy.totalTimeoutMs > 0
        && policy.maximumResponseBytes > 0 && policy.clockSkewSeconds >= 0
        && policy.maximumEvidenceAgeSeconds > 0 && policy.cacheLifetimeSeconds >= 0;
}

bool matchesIssuer(X509 *cert, X509 *issuer)
{
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(X509_get_pubkey(issuer), EVP_PKEY_free);
    return X509_check_issued(issuer, cert) == X509_V_OK && key && X509_verify(cert, key.get()) == 1;
}

bool fresh(const QDateTime &start, const QDateTime &end, const QDateTime &now, const Policy &policy)
{
    return now.isValid() && start.isValid() && start <= now.addSecs(policy.clockSkewSeconds)
        && start >= now.addSecs(-qint64(policy.maximumEvidenceAgeSeconds) - policy.clockSkewSeconds)
        && (!end.isValid() || (end >= start && end >= now.addSecs(-policy.clockSkewSeconds)));
}

bool strongSignature(const X509_ALGOR *algorithm)
{
    if (!algorithm) return false;
    const ASN1_OBJECT *object = nullptr;
    X509_ALGOR_get0(&object, nullptr, nullptr, algorithm);
    int digestNid = NID_undef, keyNid = NID_undef;
    if (!object || OBJ_find_sigid_algs(OBJ_obj2nid(object), &digestNid, &keyNid) != 1) return false;
    const auto *digest = EVP_get_digestbynid(digestNid);
    return digest && EVP_MD_size(digest) >= 32;
}

bool safeUrl(const QUrl &url)
{
    const auto scheme = url.scheme().toLower();
    return url.isValid() && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment()
        && url.toEncoded().size() <= 2048
        && url.port(scheme == QStringLiteral("https") ? 443 : 80) == (scheme == QStringLiteral("https") ? 443 : 80);
}

QList<QUrl> ocspUrls(X509 *certificate)
{
    QList<QUrl> urls;
    auto *locations = X509_get1_ocsp(certificate);
    for (int i = 0; locations && i < sk_OPENSSL_STRING_num(locations) && urls.size() < 4; ++i) {
        QUrl url(QString::fromLatin1(sk_OPENSSL_STRING_value(locations, i)), QUrl::StrictMode);
        if (!urls.contains(url)) urls << url;
    }
    X509_email_free(locations);
    return urls;
}

QList<QUrl> crlUrls(X509 *certificate)
{
    QList<QUrl> urls;
    auto *points = static_cast<CRL_DIST_POINTS *>(X509_get_ext_d2i(certificate, NID_crl_distribution_points, nullptr, nullptr));
    for (int i = 0; points && i < sk_DIST_POINT_num(points) && urls.size() < 4; ++i) {
        auto *point = sk_DIST_POINT_value(points, i);
        // Only complete, direct CRLs are supported, not partitioned/indirect CRLs.
        if (point->reasons || point->CRLissuer || !point->distpoint || point->distpoint->type != 0) continue;
        auto *names = point->distpoint->name.fullname;
        for (int j = 0; names && j < sk_GENERAL_NAME_num(names) && urls.size() < 4; ++j) {
            auto *name = sk_GENERAL_NAME_value(names, j);
            if (name->type != GEN_URI) continue;
            auto *uri = name->d.uniformResourceIdentifier;
            QUrl url(QString::fromLatin1(reinterpret_cast<const char *>(ASN1_STRING_get0_data(uri)),
                                        ASN1_STRING_length(uri)), QUrl::StrictMode);
            if (!urls.contains(url)) urls << url;
        }
    }
    CRL_DIST_POINTS_free(points);
    return urls;
}
}

Result verifyOcsp(const QByteArray &bytes, const QByteArray &certificate, const QByteArray &issuer,
                  const Policy &policy, const jpdf::crypto::ValidationTime &time)
{
    constexpr auto source = Source::Ocsp;
    ERR_clear_error();
    const auto clockError = time.clockError();
    if (!clockError.isEmpty()) return failed(source, clockError, true);
    const auto &now = time.evaluationUtc();
    auto cert = decode(certificate), authority = decode(issuer);
    if (!validPolicy(policy) || bytes.size() > policy.maximumResponseBytes || !cert || !authority
        || !matchesIssuer(cert.get(), authority.get()))
        return failed(source, QStringLiteral("Invalid policy, certificate, issuer, or response size."));
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    std::unique_ptr<OCSP_RESPONSE, decltype(&OCSP_RESPONSE_free)> response(
        d2i_OCSP_RESPONSE(nullptr, &data, bytes.size()), OCSP_RESPONSE_free);
    if (!response || data != reinterpret_cast<const unsigned char *>(bytes.constData()) + bytes.size()
        || OCSP_response_status(response.get()) != OCSP_RESPONSE_STATUS_SUCCESSFUL)
        return failed(source, QStringLiteral("Malformed or unsuccessful OCSP response."));
    std::unique_ptr<OCSP_BASICRESP, decltype(&OCSP_BASICRESP_free)> basic(
        OCSP_response_get1_basic(response.get()), OCSP_BASICRESP_free);
    if (!basic || !strongSignature(OCSP_resp_get0_tbs_sigalg(basic.get())))
        return failed(source, QStringLiteral("Unsupported or weak OCSP response signature algorithm."));
    std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> store(X509_STORE_new(), X509_STORE_free);
    if (!store || X509_STORE_add_cert(store.get(), authority.get()) != 1
        || X509_STORE_set_flags(store.get(), X509_V_FLAG_PARTIAL_CHAIN) != 1)
        return failed(source, QStringLiteral("Cannot initialize OCSP issuer verification."));
    X509_VERIFY_PARAM_set_time(X509_STORE_get0_param(store.get()), now.toSecsSinceEpoch());
    // OpenSSL validates responder authorization, including delegated OCSP-signing EKU.
    if (OCSP_basic_verify(basic.get(), nullptr, store.get(), 0) != 1)
        return failed(source, QStringLiteral("OCSP signature or responder authorization failed."));
    X509 *responder = nullptr;
    if (OCSP_resp_get0_signer(basic.get(), &responder, nullptr) != 1 || !responder)
        return failed(source, QStringLiteral("Cannot identify the authorized OCSP responder."));
    if (X509_cmp(responder, authority.get()) != 0) {
        const int index = X509_get_ext_by_NID(responder, NID_id_pkix_OCSP_noCheck, -1);
        auto *extension = index >= 0 ? X509_get_ext(responder, index) : nullptr;
        auto *value = extension ? X509_EXTENSION_get_data(extension) : nullptr;
        if (!value || X509_get_ext_by_NID(responder, NID_id_pkix_OCSP_noCheck, index) >= 0
            || ASN1_STRING_length(value) != 2 || ASN1_STRING_get0_data(value)[0] != 0x05
            || ASN1_STRING_get0_data(value)[1] != 0x00)
            return failed(source, QStringLiteral("Delegated OCSP responder requires separate revocation evidence or a valid OCSP noCheck extension."));
    }
    std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)> id(
        OCSP_cert_to_id(EVP_sha1(), cert.get(), authority.get()), OCSP_CERTID_free);
    int status = -1, reason = -1;
    ASN1_GENERALIZEDTIME *revoked = nullptr, *start = nullptr, *end = nullptr;
    if (!id || OCSP_resp_find_status(basic.get(), id.get(), &status, &reason, &revoked, &start, &end) != 1)
        return failed(source, QStringLiteral("OCSP response does not identify this certificate and issuer."));
    const int index = OCSP_resp_find(basic.get(), id.get(), -1);
    if (index < 0 || OCSP_resp_find(basic.get(), id.get(), index) >= 0)
        return failed(source, QStringLiteral("OCSP response has missing or duplicate certificate statuses."));
    auto *single = OCSP_resp_get0(basic.get(), index);
    if (OCSP_BASICRESP_get_ext_by_critical(basic.get(), 1, -1) >= 0
        || OCSP_SINGLERESP_get_ext_by_critical(single, 1, -1) >= 0)
        return failed(source, QStringLiteral("Unsupported critical OCSP extension."));
    Result result;
    result.source = source;
    result.thisUpdate = asn1DateTime(start);
    result.nextUpdate = asn1DateTime(end);
    const auto produced = asn1DateTime(OCSP_resp_get0_produced_at(basic.get()));
    if (!fresh(result.thisUpdate, result.nextUpdate, now, policy) || (end && !result.nextUpdate.isValid())
        || !produced.isValid() || produced > now.addSecs(policy.clockSkewSeconds)
        || produced < result.thisUpdate.addSecs(-policy.clockSkewSeconds))
        return failed(source, QStringLiteral("OCSP response is stale or has invalid/future evidence dates."));
    result.status = status == V_OCSP_CERTSTATUS_GOOD ? Status::Good
        : status == V_OCSP_CERTSTATUS_REVOKED ? Status::Revoked : Status::Unknown;
    if (result.status == Status::Revoked) {
        result.revokedAt = asn1DateTime(revoked);
        result.revocationReason = reason;
        if (!result.revokedAt.isValid() || result.revokedAt > now.addSecs(policy.clockSkewSeconds))
            return failed(source, QStringLiteral("OCSP revocation date is malformed or in the future."));
    }
    return result;
}

Result verifyCrl(const QByteArray &bytes, const QByteArray &certificate, const QByteArray &issuer,
                 const Policy &policy, const jpdf::crypto::ValidationTime &time)
{
    constexpr auto source = Source::Crl;
    ERR_clear_error();
    const auto clockError = time.clockError();
    if (!clockError.isEmpty()) return failed(source, clockError, true);
    const auto &now = time.evaluationUtc();
    auto cert = decode(certificate), authority = decode(issuer);
    if (!validPolicy(policy) || bytes.size() > policy.maximumResponseBytes || !cert || !authority
        || !matchesIssuer(cert.get(), authority.get()))
        return failed(source, QStringLiteral("Invalid policy, certificate, issuer, or response size."));
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    std::unique_ptr<X509_CRL, decltype(&X509_CRL_free)> crl(d2i_X509_CRL(nullptr, &data, bytes.size()), X509_CRL_free);
    if (!crl) {
        std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(bytes.constData(), bytes.size()), BIO_free);
        if (input) crl.reset(PEM_read_bio_X509_CRL(input.get(), nullptr,
            [](char *, int, int, void *) { return 0; }, nullptr));
    } else if (data != reinterpret_cast<const unsigned char *>(bytes.constData()) + bytes.size()) {
        crl.reset();
    }
    if (!crl || X509_NAME_cmp(X509_CRL_get_issuer(crl.get()), X509_get_subject_name(authority.get())) != 0)
        return failed(source, QStringLiteral("Malformed CRL or incorrect issuer."));
    if (X509_CRL_get_ext_by_NID(crl.get(), NID_delta_crl, -1) >= 0
        || X509_CRL_get_ext_by_NID(crl.get(), NID_issuing_distribution_point, -1) >= 0)
        return failed(source, QStringLiteral("Delta, scoped, and indirect CRLs are not supported."));
    for (int i = 0; i < X509_CRL_get_ext_count(crl.get()); ++i)
        if (X509_EXTENSION_get_critical(X509_CRL_get_ext(crl.get(), i)))
            return failed(source, QStringLiteral("Unsupported critical CRL extension."));
    const uint32_t usage = X509_get_key_usage(authority.get());
    if (usage != UINT32_MAX && !(usage & KU_CRL_SIGN))
        return failed(source, QStringLiteral("The issuer certificate does not allow CRL signing."));
    const X509_ALGOR *algorithm = nullptr;
    X509_CRL_get0_signature(crl.get(), nullptr, &algorithm);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(X509_get_pubkey(authority.get()), EVP_PKEY_free);
    if (!key || !strongSignature(algorithm) || X509_CRL_verify(crl.get(), key.get()) != 1)
        return failed(source, QStringLiteral("CRL signature verification failed or its algorithm is unsupported/weak."));
    Result result;
    result.source = source;
    result.thisUpdate = asn1DateTime(X509_CRL_get0_lastUpdate(crl.get()));
    result.nextUpdate = asn1DateTime(X509_CRL_get0_nextUpdate(crl.get()));
    if (!result.nextUpdate.isValid() || !fresh(result.thisUpdate, result.nextUpdate, now, policy))
        return failed(source, QStringLiteral("CRL is stale or has invalid/future evidence dates."));
    auto *entries = X509_CRL_get_REVOKED(crl.get());
    for (int i = 0; entries && i < sk_X509_REVOKED_num(entries); ++i) {
        auto *entry = sk_X509_REVOKED_value(entries, i);
        if (X509_REVOKED_get_ext_by_NID(entry, NID_certificate_issuer, -1) >= 0)
            return failed(source, QStringLiteral("Indirect CRL entries are not supported."));
        for (int j = 0; j < X509_REVOKED_get_ext_count(entry); ++j)
            if (X509_EXTENSION_get_critical(X509_REVOKED_get_ext(entry, j)))
                return failed(source, QStringLiteral("Unsupported critical CRL-entry extension."));
    }
    X509_REVOKED *entry = nullptr;
    const int listed = X509_CRL_get0_by_serial(crl.get(), &entry, X509_get_serialNumber(cert.get()));
    if (listed == 2)
        return failed(source, QStringLiteral("removeFromCRL requires unsupported delta-CRL processing."));
    result.status = listed == 1 ? Status::Revoked : Status::Good;
    if (result.status == Status::Revoked) {
        result.revokedAt = asn1DateTime(X509_REVOKED_get0_revocationDate(entry));
        auto *reason = static_cast<ASN1_ENUMERATED *>(X509_REVOKED_get_ext_d2i(entry, NID_crl_reason, nullptr, nullptr));
        if (reason) { result.revocationReason = ASN1_ENUMERATED_get(reason); ASN1_ENUMERATED_free(reason); }
        auto *invalidity = static_cast<ASN1_GENERALIZEDTIME *>(X509_REVOKED_get_ext_d2i(entry, NID_invalidity_date, nullptr, nullptr));
        if (invalidity) { result.invalidityDate = asn1DateTime(invalidity); ASN1_GENERALIZEDTIME_free(invalidity); }
        if (!result.revokedAt.isValid() || result.revokedAt > now.addSecs(policy.clockSkewSeconds)
            || result.revocationReason == CRL_REASON_REMOVE_FROM_CRL)
            return failed(source, QStringLiteral("Invalid revocation entry or unsupported removeFromCRL status."));
    }
    return result;
}

struct Checker::Impl {
    struct Cached { QByteArray body; QDateTime fetched; };
    std::shared_ptr<Transport> transport;
    Policy policy;
    QHash<QByteArray, Cached> cache;
    int cachedBytes = 0;
};

Checker::Checker(std::shared_ptr<Transport> transport, Policy policy)
    : impl_(std::make_unique<Impl>())
{
    impl_->transport = std::move(transport);
    impl_->policy = policy;
}
Checker::~Checker() = default;
void Checker::clearCache() { impl_->cache.clear(); impl_->cachedBytes = 0; }

Result Checker::check(const QByteArray &certificate, const QByteArray &issuer,
                      const jpdf::crypto::ValidationTime &time)
{
    ERR_clear_error();
    const auto clockError = time.clockError();
    if (!clockError.isEmpty()) return failed(Source::None, clockError, true);
    const auto &now = time.systemUtc();
    if (impl_->policy.cancelled && impl_->policy.cancelled())
        return failed(Source::None, QStringLiteral("Revocation check cancelled."));
    auto cert = decode(certificate), authority = decode(issuer);
    if (!validPolicy(impl_->policy) || !cert || !authority || !matchesIssuer(cert.get(), authority.get()))
        return failed(Source::None, QStringLiteral("Invalid policy or certificate/issuer pair."));
    if (!impl_->transport) return failed(Source::None, QStringLiteral("No revocation transport configured."));
    std::unique_ptr<OCSP_REQUEST, decltype(&OCSP_REQUEST_free)> request(OCSP_REQUEST_new(), OCSP_REQUEST_free);
    auto *id = OCSP_cert_to_id(EVP_sha1(), cert.get(), authority.get());
    if (!request || !id || !OCSP_request_add0_id(request.get(), id)) {
        OCSP_CERTID_free(id);
        return failed(Source::Ocsp, QStringLiteral("Cannot build OCSP request."));
    }
    const int size = i2d_OCSP_REQUEST(request.get(), nullptr);
    if (size <= 0) return failed(Source::Ocsp, QStringLiteral("Cannot encode OCSP request."));
    QByteArray encoded(size, '\0');
    auto *data = reinterpret_cast<unsigned char *>(encoded.data());
    i2d_OCSP_REQUEST(request.get(), &data);
    // No nonce: supports pre-produced/cached OCSP, with strict signed freshness checks.
    QElapsedTimer elapsed;
    elapsed.start();
    QStringList diagnostics;
    Result unknown;
    bool sawUnknown = false, attempted = false;
    for (const auto source : {Source::Ocsp, Source::Crl}) {
        const auto urls = source == Source::Ocsp ? ocspUrls(cert.get()) : crlUrls(cert.get());
        for (const auto &url : urls) {
            if (impl_->policy.cancelled && impl_->policy.cancelled())
                return failed(source, QStringLiteral("Revocation check cancelled."));
            attempted = true;
            if (!safeUrl(url)) { diagnostics << QStringLiteral("Rejected unsafe revocation URL: %1").arg(url.adjusted(QUrl::RemoveUserInfo).toString()); continue; }
            const auto key = QCryptographicHash::hash(certificate + issuer + url.toEncoded()
                + QByteArray(1, source == Source::Ocsp ? 'O' : 'C'), QCryptographicHash::Sha256);
            const auto validate = [&](const QByteArray &body) {
                return source == Source::Ocsp ? verifyOcsp(body, certificate, issuer, impl_->policy, time)
                                              : verifyCrl(body, certificate, issuer, impl_->policy, time);
            };
            const auto cached = impl_->cache.constFind(key);
            Result result;
            bool fromCache = false;
            if (cached != impl_->cache.cend()
                && cached->fetched.secsTo(now) >= 0
                && cached->fetched.secsTo(now) < impl_->policy.cacheLifetimeSeconds) {
                result = validate(cached->body);
                fromCache = result.status != Status::CheckFailed;
            }
            if (!fromCache) {
                const int remaining = impl_->policy.totalTimeoutMs - int(elapsed.elapsed());
                if (remaining <= 0) { diagnostics << QStringLiteral("Revocation check exceeded its total time budget."); break; }
                auto response = impl_->transport->fetch(url, source == Source::Ocsp ? encoded : QByteArray(),
                    qMin(remaining, impl_->policy.requestTimeoutMs), impl_->policy.maximumResponseBytes, impl_->policy.cancelled);
                if (impl_->policy.cancelled && impl_->policy.cancelled())
                    return failed(source, QStringLiteral("Revocation check cancelled."));
                if (elapsed.elapsed() > impl_->policy.totalTimeoutMs) {
                    diagnostics << QStringLiteral("Revocation check exceeded its total time budget.");
                    break;
                }
                if (!response.error.isEmpty()) { diagnostics << response.error; continue; }
                result = validate(response.body);
                if (result.status != Status::CheckFailed && impl_->policy.cacheLifetimeSeconds > 0
                    && response.body.size() <= 8 * 1024 * 1024) {
                    if (impl_->cache.size() >= 16 || impl_->cachedBytes + response.body.size() > 8 * 1024 * 1024) clearCache();
                    const auto old = impl_->cache.find(key);
                    if (old != impl_->cache.end()) impl_->cachedBytes -= old->body.size();
                    impl_->cachedBytes += response.body.size();
                    impl_->cache.insert(key, {response.body, now});
                }
            }
            diagnostics << result.diagnostics;
            if (result.status == Status::Good || result.status == Status::Revoked) {
                result.diagnostics = diagnostics;
                return result;
            }
            if (result.status == Status::Unknown) { unknown = result; sawUnknown = true; }
        }
    }
    if (sawUnknown) { unknown.diagnostics = diagnostics; return unknown; }
    Result result;
    result.status = attempted ? Status::CheckFailed : Status::NotChecked;
    result.diagnostics = diagnostics;
    if (!attempted) result.diagnostics << QStringLiteral("No supported OCSP or complete/direct CRL endpoint is advertised.");
    return result;
}
} // namespace jpdf::revocation
