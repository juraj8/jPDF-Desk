#include "jpdf_desk/revocation/http_transport.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <atomic>

using namespace jpdf::revocation;
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    for (const auto *blocked : {"0.0.0.0", "127.0.0.1", "10.0.0.1", "172.16.1.1", "192.168.1.1",
         "169.254.169.254", "100.64.0.1", "192.0.2.1", "198.18.0.1", "198.51.100.1", "203.0.113.1",
         "224.0.0.1", "255.255.255.255", "::", "::1", "::ffff:127.0.0.1", "fc00::1", "fe80::1",
         "2001:db8::1", "2002:7f00:1::1", "3fff::1"})
        if (isPublicRevocationAddress(QHostAddress(QString::fromLatin1(blocked)))) return 1;
    for (const auto *allowed : {"8.8.8.8", "1.1.1.1", "2606:4700:4700::1111", "::ffff:8.8.8.8"})
        if (!isPublicRevocationAddress(QHostAddress(QString::fromLatin1(allowed)))) return 2;
    auto transport = makeHttpTransport();
    // These must fail before any network access; tests never contact the internet.
    for (const auto *blocked : {"http://127.0.0.1/", "http://[::1]/", "http://169.254.169.254/",
         "http://localhost/", "http://server.local/", "file:///etc/passwd", "ftp://8.8.8.8/",
         "http://user:secret@8.8.8.8/", "http://8.8.8.8:8080/", "http://8.8.8.8/#fragment"}) {
        const auto result = transport->fetch(QUrl(QString::fromLatin1(blocked)), {}, 100, 1024);
        if (result.error.isEmpty() || !result.body.isEmpty()) return 3;
    }
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 4;
    int requests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, &server, [&, socket] {
                auto input = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", input);
                if (!input.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
                socket->setProperty("handled", true);
                ++requests;
                const auto path = input.split(' ').value(1);
                if (path == "/wait") return;
                QByteArray response;
                if (path == "/redirect")
                    response = "HTTP/1.1 302 Found\r\nLocation: /ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                else if (path == "/big")
                    response = "HTTP/1.1 200 OK\r\nContent-Length: 2048\r\nConnection: close\r\n\r\n" + QByteArray(2048, 'x');
                else if (path == "/chunked")
                    response = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n800\r\n"
                        + QByteArray(2048, 'x') + "\r\n0\r\n\r\n";
                else if (path == "/compressed")
                    response = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
                else
                    response = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });
    HttpTransportOptions options;
    options.approvedPrivateAddresses << QHostAddress::LocalHost;
    options.additionalPorts << server.serverPort();
    auto local = makeHttpTransport(options); // Exceptions exist only in this test instance.
    const auto url = [&](const QString &path) {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    };
    const auto ok = local->fetch(url("/ok"), {}, 1000, 128);
    if (!ok.error.isEmpty() || ok.body != "abc") return 5;
    const int beforeRedirect = requests;
    const auto redirect = local->fetch(url("/redirect"), {}, 1000, 128);
    if (redirect.error.isEmpty() || requests != beforeRedirect + 1) return 6;
    for (const auto *path : {"/big", "/chunked", "/compressed"}) {
        const auto response = local->fetch(url(QString::fromLatin1(path)), {}, 1000, 128);
        if (response.error.isEmpty() || !response.body.isEmpty()) return 7;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    const auto timeout = local->fetch(url("/wait"), {}, 100, 128);
    if (timeout.error.isEmpty() || elapsed.elapsed() > 2000) return 8;
    std::atomic<bool> cancelled{false};
    QTimer cancellation;
    cancellation.setSingleShot(true);
    QObject::connect(&cancellation, &QTimer::timeout, &server, [&] { cancelled.store(true); });
    cancellation.start(30);
    elapsed.restart();
    const auto interrupted = local->fetch(url("/wait"), {}, 5000, 128, [&] { return cancelled.load(); });
    if (!interrupted.error.contains("cancelled") || elapsed.elapsed() > 2000) return 9;
    const int beforeCancelled = requests;
    const auto immediate = local->fetch(url("/ok"), {}, 1000, 128, [] { return true; });
    if (!immediate.error.contains("cancelled") || requests != beforeCancelled) return 10;
    if (transport->fetch(url("/ok"), {}, 100, 128).error.isEmpty()) return 11;
    return 0;
}
