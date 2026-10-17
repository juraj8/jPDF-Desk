#include "jpdf_desk/revocation/http_transport.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <utility>
#ifdef JPDF_REVOCATION_STATIC_TLS
#include <QtPlugin>
Q_IMPORT_PLUGIN(QTlsBackendOpenSSL)
#endif

namespace jpdf::revocation {
bool isPublicRevocationAddress(const QHostAddress &address)
{
    bool ipv4 = false;
    const quint32 value = address.toIPv4Address(&ipv4);
    if (ipv4) {
        // Include private, loopback, link-local, shared, benchmark, documentation,
        // protocol-assignment, multicast and reserved ranges.
        static const struct { quint32 network; int bits; } blocked[] = {
            {0x00000000, 8}, {0x0a000000, 8}, {0x64400000, 10}, {0x7f000000, 8},
            {0xa9fe0000, 16}, {0xac100000, 12}, {0xc0000000, 24}, {0xc0000200, 24},
            {0xc0586300, 24}, {0xc0a80000, 16}, {0xc6120000, 15}, {0xc6336400, 24},
            {0xcb007100, 24}, {0xe0000000, 3}
        };
        for (const auto &range : blocked)
            if ((value >> (32 - range.bits)) == (range.network >> (32 - range.bits))) return false;
        return true;
    }
    if (address.protocol() != QAbstractSocket::IPv6Protocol || !address.scopeId().isEmpty()) return false;
    if (!address.isInSubnet(QHostAddress(QStringLiteral("2000::")), 3)) return false;
    for (const auto &range : {QStringLiteral("2001::/23"), QStringLiteral("2001:db8::/32"),
                              QStringLiteral("2002::/16"), QStringLiteral("3fff::/20")})
        if (address.isInSubnet(QHostAddress::parseSubnet(range))) return false;
    return true;
}

namespace {
class HttpTransport final : public Transport {
public:
    explicit HttpTransport(HttpTransportOptions options) : options_(std::move(options)) {}
    Response fetch(const QUrl &url, const QByteArray &ocspRequest, int timeoutMs, int maximumBytes,
                   const Cancellation &cancelled) override
    {
        if (cancelled && cancelled()) return {{}, QStringLiteral("Revocation request cancelled.")};
        const auto allowed = [&](const QHostAddress &address) {
            return isPublicRevocationAddress(address) || options_.approvedPrivateAddresses.contains(address);
        };
        const auto scheme = url.scheme().toLower();
        const auto host = url.host().toLower();
        const int port = scheme == QStringLiteral("https") ? 443 : 80;
        if (timeoutMs <= 0 || maximumBytes <= 0 || !url.isValid()
            || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
            || !url.userInfo().isEmpty() || url.hasFragment() || url.toEncoded().size() > 2048
            || host.isEmpty() || host.contains(QLatin1Char('%'))
            || (url.port(port) != port && !options_.additionalPorts.contains(url.port(port))))
            return {{}, QStringLiteral("Rejected unsafe revocation URL or transport policy.")};
        QElapsedTimer elapsed;
        elapsed.start();
        QHostAddress address(host);
        if (address.isNull()) {
            if (!host.contains(QLatin1Char('.')) || host.endsWith(QStringLiteral(".localhost"))
                || host.endsWith(QStringLiteral(".local")) || host.endsWith(QStringLiteral(".internal")))
                return {{}, QStringLiteral("Local revocation hostnames are forbidden.")};
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            bool resolved = false;
            QHostInfo information;
            const int lookup = QHostInfo::lookupHost(host, &loop, [&](const QHostInfo &result) {
                information = result;
                resolved = true;
                loop.quit();
            });
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QTimer cancellationTimer;
            QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&] {
                if (cancelled && cancelled()) loop.quit();
            });
            cancellationTimer.start(25);
            timer.start(timeoutMs);
            loop.exec();
            if (!resolved) {
                QHostInfo::abortHostLookup(lookup);
                return {{}, cancelled && cancelled() ? QStringLiteral("Revocation request cancelled.")
                    : QStringLiteral("Revocation DNS lookup timed out.")};
            }
            if (information.error() != QHostInfo::NoError || information.addresses().isEmpty())
                return {{}, QStringLiteral("Revocation DNS lookup failed: %1").arg(information.errorString())};
            // Reject mixed public/private answers, not just the chosen address.
            for (const auto &candidate : information.addresses())
                if (!allowed(candidate))
                    return {{}, QStringLiteral("Revocation DNS resolved to a non-public address.")};
            address = information.addresses().first();
        }
        if (cancelled && cancelled()) return {{}, QStringLiteral("Revocation request cancelled.")};
        if (!allowed(address))
            return {{}, QStringLiteral("Non-public revocation destinations are forbidden.")};
        const int remaining = timeoutMs - int(elapsed.elapsed());
        if (remaining <= 0) return {{}, QStringLiteral("Revocation request timed out.")};
        // Pin the resolved IP: no second DNS lookup, hence no DNS rebinding race.
        QUrl pinned = url;
        pinned.setHost(address.toString());
        QNetworkRequest request(pinned);
        request.setPeerVerifyName(host); // TLS peer verification and SNI retain the original identity.
        QByteArray authority = host.contains(QLatin1Char(':'))
            ? '[' + host.toLatin1() + ']' : QUrl::toAce(host);
        if (url.port() != -1) authority += ':' + QByteArray::number(url.port());
        request.setRawHeader("Host", authority);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(remaining);
        request.setRawHeader("Accept", ocspRequest.isEmpty() ? "application/pkix-crl" : "application/ocsp-response");
        request.setRawHeader("Accept-Encoding", "identity"); // Do not decompress untrusted response bodies.
        if (!ocspRequest.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, "application/ocsp-request");
        QNetworkAccessManager manager;
        manager.setProxy(QNetworkProxy::NoProxy);
        auto *reply = ocspRequest.isEmpty() ? manager.get(request) : manager.post(request, ocspRequest);
        reply->setReadBufferSize(qint64(maximumBytes) + 1);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        Response response;
        const auto read = [&] {
            if (!reply->isOpen()) return;
            response.body += reply->read(qint64(maximumBytes) - response.body.size() + 1);
            if (response.body.size() > maximumBytes) {
                response.error = QStringLiteral("Revocation response exceeds the size limit.");
                reply->abort();
            }
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
        QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&] {
            const auto encoding = reply->rawHeader("Content-Encoding").trimmed().toLower();
            if (!encoding.isEmpty() && encoding != "identity") {
                response.error = QStringLiteral("Compressed revocation responses are forbidden.");
                reply->abort();
            }
            if (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > maximumBytes) {
                response.error = QStringLiteral("Revocation response exceeds the size limit.");
                reply->abort();
            }
        });
        QObject::connect(&manager, &QNetworkAccessManager::authenticationRequired, &loop,
                         [&](QNetworkReply *requestReply, QAuthenticator *) { requestReply->abort(); });
        QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
            response.error = QStringLiteral("Revocation request timed out.");
            reply->abort();
            loop.quit();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer cancellationTimer;
        QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&] {
            if (cancelled && cancelled()) {
                response.error = QStringLiteral("Revocation request cancelled.");
                reply->abort();
                loop.quit();
            }
        });
        cancellationTimer.start(25);
        timer.start(remaining);
        if (!reply->isFinished()) loop.exec();
        read();
        if (cancelled && cancelled()) response.error = QStringLiteral("Revocation request cancelled.");
        if (response.error.isEmpty() && reply->error() != QNetworkReply::NoError)
            response.error = QStringLiteral("Revocation HTTP request failed: %1").arg(reply->errorString());
        if (response.error.isEmpty() && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            response.error = QStringLiteral("Revocation server did not return HTTP 200; redirects are forbidden.");
        if (!response.error.isEmpty()) response.body.clear();
        delete reply;
        return response;
    }
private:
    HttpTransportOptions options_;
};
}

std::shared_ptr<Transport> makeHttpTransport(HttpTransportOptions options)
{
    return std::make_shared<HttpTransport>(std::move(options));
}
} // namespace jpdf::revocation
