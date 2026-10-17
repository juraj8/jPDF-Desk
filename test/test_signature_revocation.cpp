#include "signature_revocation.h"
#include "pdf_task.h"
#include "jpdf_desk/signing/openssl_provider.h"
#include "jpdf_desk/revocation/revocation.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"
#include "ui/signature_check_dialog.h"
#include <QLabel>

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <iostream>

namespace {
class OfflineTransport final : public jpdf::revocation::Transport {
public:
    int calls = 0;
    jpdf::revocation::Response fetch(const QUrl &, const QByteArray &, int, int,
                                    const jpdf::revocation::Cancellation &) override
    {
        ++calls;
        return {{}, "Test offline"};
    }
};
class BlockingTransport final : public jpdf::revocation::Transport {
public:
    std::atomic<bool> active{false}, stopped{false};
    jpdf::revocation::Response fetch(const QUrl &, const QByteArray &, int timeout, int,
                                    const jpdf::revocation::Cancellation &cancelled) override
    {
        active.store(true);
        for (int elapsed = 0; elapsed < timeout && !(cancelled && cancelled()); elapsed += 5)
            QThread::msleep(5);
        stopped.store(true);
        return {{}, "Cancelled"};
    }
};
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 1;
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;
    const auto path = [&](const QString &name) { return directory.filePath(name); };
    const auto run = [&](const QStringList &arguments) {
        QProcess process;
        process.start(QString::fromLocal8Bit(argv[1]), arguments);
        const bool ok = process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0;
        if (!ok) std::cerr << process.readAllStandardError().constData() << '\n';
        return ok;
    };
    if (!write(path("extensions.cnf"),
        "[ca]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n"
        "authorityInfoAccess=OCSP;URI:http://ocsp.example.test\n"
        "[leaf]\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n"
        "extendedKeyUsage=emailProtection\nauthorityInfoAccess=OCSP;URI:http://ocsp.example.test\n")
        || !write(path("message.txt"), "Test signature")) return 3;
    if (!run({"req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", path("root.key"),
              "-out", path("root.pem"), "-subj", "/CN=Revocation Test Root", "-days", "1",
              "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign"})) return 4;
    for (const auto &name : {QStringLiteral("intermediate"), QStringLiteral("leaf")}) {
        const auto issuer = name == "intermediate" ? QStringLiteral("root") : QStringLiteral("intermediate");
        if (!run({"req", "-new", "-newkey", "rsa:2048", "-nodes", "-keyout", path(name + ".key"),
                  "-out", path(name + ".csr"), "-subj", "/CN=" + name})
            || !run({"x509", "-req", "-in", path(name + ".csr"), "-CA", path(issuer + ".pem"),
                     "-CAkey", path(issuer + ".key"), "-set_serial", name == "leaf" ? "3" : "2",
                     "-days", "1", "-out", path(name + ".pem"), "-extfile", path("extensions.cnf"),
                     "-extensions", name == "leaf" ? "leaf" : "ca"})) return 5;
    }
    if (!run({"smime", "-sign", "-binary", "-in", path("message.txt"), "-signer", path("leaf.pem"),
              "-inkey", path("leaf.key"), "-certfile", path("intermediate.pem"), "-outform", "DER",
              "-out", path("signature.der")})) return 6;
    qputenv("SSL_CERT_FILE", QFile::encodeName(path("root.pem")));
    qputenv("SSL_CERT_DIR", QFile::encodeName(directory.path()));
    const auto services = openSslSignatureServices();
    const auto metadata = services.verification->certificateDetails(read(path("signature.der")));
    if (metadata.systemChain != CertificateChainValidation::Accepted || metadata.signerChain.size() != 3
        || !metadata.signerChain[0].subject.contains("leaf")
        || !metadata.signerChain[1].subject.contains("intermediate")
        || !metadata.signerChain[2].subject.contains("Root")) return 7;
    const auto expiryTime = jpdf::crypto::ValidationTime::atSystemTime(QDateTime::currentDateTimeUtc().addDays(2));
    const auto expiredMetadata = services.verification->certificateDetails(read(path("signature.der")), expiryTime);
    if (expiredMetadata.systemChain != CertificateChainValidation::Failed
        || !expiredMetadata.signerChain.isEmpty() || expiredMetadata.problems.isEmpty()) return 15;
    DigitalSignatureStatus signature;
    signature.signedField = true;
    signature.signerChain = metadata.signerChain;
    // Reuse the validated three-certificate fixture to exercise both chain loops.
    // Actual TSA-purpose path export is covered by the timestamp-verification test.
    signature.timestamp.status = SignatureTimestamp::Status::Verified;
    signature.timestamp.certificateChain = metadata.signerChain;
    QVector<DigitalSignatureStatus> signatures{signature};
    auto transport = std::make_shared<OfflineTransport>();
    PdfTaskProgress progress;
    const auto operationTime = jpdf::crypto::ValidationTime{};
    checkSignatureRevocation(signatures, transport, progress, operationTime);
    if (signatures[0].revocationTime != operationTime.systemUtc()) return 16;
    if (!signatures[0].revocationAttempted || signatures[0].revocationChecks.size() != 4
        || transport->calls != 4 || progress.completed() != 4 || progress.total() != 4) return 8;
    for (int i = 0; i < 4; ++i) {
        const auto &check = signatures[0].revocationChecks[i];
        if (check.result.status != jpdf::revocation::Status::CheckFailed
            || check.timestampAuthority != (i >= 2) || check.depth != i % 2
            || check.subject.contains("Root")) return 9;
    }
    const auto badTime = jpdf::crypto::ValidationTime::atSystemTime(
        jpdf::crypto::detail::validationClockBounds().build);
    QVector<DigitalSignatureStatus> clockBlocked{signature};
    checkSignatureRevocation(clockBlocked, transport, progress, badTime);
    if (transport->calls != 4 || clockBlocked[0].revocationChecks.size() != 1
        || !clockBlocked[0].revocationChecks.first().result.clockError) return 17;
    SignatureCheckDialog clockReport(clockBlocked, "Test policy");
    const auto *clockSummary = clockReport.findChild<QLabel *>("signatureOverallStatus1");
    const auto *clockRevocation = clockReport.findChild<QLabel *>("signatureRevocationStatus1");
    if (!clockSummary || !clockSummary->text().contains("System clock appears incorrect")
        || clockSummary->text().contains("Invalid") || !clockRevocation
        || !clockRevocation->text().contains("Correct the date and time")) return 18;
    auto offlineBlocked = signature;
    offlineBlocked.clockError = "Offline clock check failed; recheck required";
    QVector<DigitalSignatureStatus> needsRecheck{offlineBlocked};
    checkSignatureRevocation(needsRecheck, transport, progress, operationTime);
    if (transport->calls != 4 || needsRecheck[0].revocationChecks.size() != 1
        || !needsRecheck[0].revocationChecks[0].result.clockError) return 19;
    auto unavailable = signature;
    unavailable.signerChain.clear();
    unavailable.timestamp.certificateChain.clear();
    QVector<DigitalSignatureStatus> skipped{unavailable};
    const int before = transport->calls;
    checkSignatureRevocation(skipped, transport, progress);
    if (transport->calls != before || skipped[0].revocationChecks.size() != 2) return 10;
    for (const auto &check : skipped[0].revocationChecks)
        if (check.result.status != jpdf::revocation::Status::NotChecked) return 11;
    auto anchor = signature;
    anchor.signerChain = {metadata.signerChain.last()};
    anchor.timestamp.status = SignatureTimestamp::Status::Absent;
    QVector<DigitalSignatureStatus> anchors{anchor};
    checkSignatureRevocation(anchors, transport, progress);
    if (transport->calls != before || anchors[0].revocationChecks.size() != 1
        || !anchors[0].revocationChecks[0].trustAnchorOnly) return 13;
    auto failedTimestamp = signature;
    failedTimestamp.timestamp.status = SignatureTimestamp::Status::Failed;
    QVector<DigitalSignatureStatus> noTsaPath{failedTimestamp};
    checkSignatureRevocation(noTsaPath, transport, progress);
    if (transport->calls != before + 2 || noTsaPath[0].revocationChecks.size() != 3
        || noTsaPath[0].revocationChecks.last().result.status != jpdf::revocation::Status::NotChecked
        || !noTsaPath[0].revocationChecks.last().timestampAuthority) return 14;
    auto blocking = std::make_shared<BlockingTransport>();
    int heartbeat = 0;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] { ++heartbeat; });
    tick.start(1);
    QTimer cancellation;
    QObject::connect(&cancellation, &QTimer::timeout, [&] {
        if (!blocking->active.load()) return;
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (dialog && dialog->objectName() == "pdfTaskDialog") dialog->reject();
    });
    cancellation.start(10);
    const auto previous = signatures[0].revocationChecks[0].result.diagnostics;
    if (runOnlineSignatureRevocation(nullptr, signatures, blocking) || !blocking->stopped.load()
        || heartbeat == 0 || signatures[0].revocationChecks.size() != 4
        || signatures[0].revocationChecks[0].result.diagnostics != previous) return 12;
    return 0;
}
