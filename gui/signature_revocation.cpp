#include "signature_revocation.h"
#include "pdf_task.h"
#include "jpdf_desk/revocation/http_transport.h"

#include <QObject>
#include <utility>

namespace {
int certificateCount(const ValidatedCertificateChain &chain)
{
    return qMax(0, int(chain.size()) - 1); // Trusted anchor is not queried.
}
}

void checkSignatureRevocation(QVector<DigitalSignatureStatus> &signatures,
                              const std::shared_ptr<jpdf::revocation::Transport> &transport,
                              const PdfTaskProgress &progress,
                              const jpdf::crypto::ValidationTime &time)
{
    using namespace jpdf::revocation;
    const auto clockError = time.clockError();
    Policy policy;
    policy.cancelled = [&progress] { return progress.cancelled(); };
    Checker checker(transport, policy);
    int total = 0, completed = 0;
    for (const auto &signature : signatures) {
        if (!signature.signedField || !signature.error.isEmpty()) continue;
        total += certificateCount(signature.signerChain);
        if (signature.timestamp.status == SignatureTimestamp::Status::Verified)
            total += certificateCount(signature.timestamp.certificateChain);
    }
    progress.report(0, total);
    for (auto &signature : signatures) {
        progress.checkCancelled();
        if (!signature.signedField || !signature.error.isEmpty()) continue;
        signature.revocationAttempted = true;
        signature.revocationTime = time.systemUtc();
        signature.revocationChecks.clear();
        const auto blockedClock = clockError.isEmpty() ? signature.clockError : clockError;
        if (!blockedClock.isEmpty()) {
            SignatureRevocationCheck blocked;
            blocked.result.status = Status::CheckFailed;
            blocked.result.clockError = true;
            blocked.result.diagnostics << blockedClock;
            signature.revocationChecks.push_back(blocked);
            continue;
        }
        const auto checkChain = [&](const ValidatedCertificateChain &chain, bool tsa) {
            if (chain.size() < 2) {
                SignatureRevocationCheck skipped;
                skipped.timestampAuthority = tsa;
                skipped.trustAnchorOnly = chain.size() == 1;
                if (!chain.isEmpty()) skipped.subject = chain.first().subject;
                skipped.result.diagnostics << (chain.isEmpty()
                    ? QObject::tr("No validated system/OpenSSL path is available; no issuer URLs were contacted.")
                    : QObject::tr("The certificate is a trusted anchor; root revocation is not queried."));
                signature.revocationChecks.push_back(skipped);
                return;
            }
            for (int depth = 0; depth + 1 < chain.size(); ++depth) {
                progress.checkCancelled();
                SignatureRevocationCheck check;
                check.timestampAuthority = tsa;
                check.depth = depth;
                check.subject = chain[depth].subject;
                check.result = checker.check(chain[depth].der, chain[depth + 1].der, time);
                progress.checkCancelled();
                signature.revocationChecks.push_back(check);
                progress.report(++completed, total);
            }
        };
        checkChain(signature.signerChain, false);
        if (signature.timestamp.status == SignatureTimestamp::Status::Verified)
            checkChain(signature.timestamp.certificateChain, true);
        else if (signature.timestamp.status != SignatureTimestamp::Status::Absent) {
            SignatureRevocationCheck skipped;
            skipped.timestampAuthority = true;
            skipped.result.diagnostics << QObject::tr("Timestamp not verified or unsupported; no validated TSA path is available. No TSA services were contacted.");
            signature.revocationChecks.push_back(skipped);
        }
    }
    progress.checkCancelled();
}

bool runOnlineSignatureRevocation(QWidget *parent, QVector<DigitalSignatureStatus> &signatures,
                                 std::shared_ptr<jpdf::revocation::Transport> transport)
{
    auto checked = signatures;
    if (!transport) transport = jpdf::revocation::makeHttpTransport();
    const bool completed = runPdfTask(parent, QObject::tr("Checking certificate revocation"),
        [&](const PdfTaskProgress &progress) { checkSignatureRevocation(checked, transport, progress); },
        QObject::tr("certificates"));
    if (completed) signatures = std::move(checked);
    return completed;
}
