#pragma once

#include "revocation_types.h"
#include "jpdf_desk/crypto/validation_time.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QUrl>
#include <memory>
#include <functional>

namespace jpdf::revocation {

using Cancellation = std::function<bool()>;

struct Policy {
    int requestTimeoutMs = 5000;
    int totalTimeoutMs = 15000;
    int maximumResponseBytes = 2 * 1024 * 1024;
    int clockSkewSeconds = 300;
    int maximumEvidenceAgeSeconds = 86400;
    int cacheLifetimeSeconds = 300;
    Cancellation cancelled;
};

struct Response {
    QByteArray body;
    QString error;
};

// Implementations must enforce the requested timeout/size limit. The supplied
// HTTP implementation also rejects private destinations, redirects and proxies.
class Transport {
public:
    virtual ~Transport() = default;
    virtual Response fetch(const QUrl &url, const QByteArray &ocspRequest,
                           int timeoutMs, int maximumBytes, const Cancellation &cancelled = {}) = 0;
};

// Synchronous, single-thread use: run online checks on a worker, not the GUI.
// Certificates are DER. The caller must independently validate the issuer chain.
// This module verifies evidence against that issuer, not the issuer's root trust.
class Checker {
public:
    explicit Checker(std::shared_ptr<Transport> transport, Policy policy = {});
    ~Checker();
    Checker(const Checker &) = delete;
    Checker &operator=(const Checker &) = delete;
    Result check(const QByteArray &certificate, const QByteArray &issuer,
                 const jpdf::crypto::ValidationTime &time = {});
    void clearCache();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Offline validators: freshness uses time.evaluationUtc() (system UTC by default).
// Passing a QDateTime captures the real system clock separately and uses that date
// for evidence evaluation; a past date does NOT establish historical validity.
// All checks fail if time.systemUtc() is not later than the embedded/build bounds.
Result verifyOcsp(const QByteArray &response, const QByteArray &certificate,
                  const QByteArray &issuer, const Policy &policy = {},
                  const jpdf::crypto::ValidationTime &time = {});
Result verifyCrl(const QByteArray &response, const QByteArray &certificate,
                 const QByteArray &issuer, const Policy &policy = {},
                 const jpdf::crypto::ValidationTime &time = {});

} // namespace jpdf::revocation
