#include "ui/metadata_dialog.h"
#include "ui/password_dialog.h"
#include "ui/signature_check_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QGroupBox>
#include <QToolButton>
#include <stdexcept>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MetadataDialog metadata({{"Title", "Original"}, {"Subject", "Subject"},
                             {"CreationDate", "D:20250101"}});
    if (!metadata.changes().isEmpty()) return 1;
    auto *title = metadata.findChild<QLineEdit *>("metadataTitle");
    auto *created = metadata.findChild<QLineEdit *>("metadataCreationDate");
    auto *subject = metadata.findChild<QPlainTextEdit *>("metadataSubject");
    if (!title || !created || !subject || !created->isReadOnly()) return 2;
    title->clear(); // Empty values must be included, not treated as unchanged.
    subject->setPlainText("Updated\nsubject");
    if (metadata.changes() != QMap<QString, QString>{{"Title", ""}, {"Subject", "Updated\nsubject"}})
        return 3;
    title->setText("Original");
    subject->setPlainText("Subject");
    if (!metadata.changes().isEmpty()) return 4;

    int calls = 0;
    QString applied;
    bool fail = true;
    PasswordDialog password([&](const QString &value) {
        ++calls;
        if (fail) throw std::runtime_error("Rejected by document");
        applied = value;
    });
    auto *entry = password.findChild<QLineEdit *>("newPassword");
    auto *confirmation = password.findChild<QLineEdit *>("confirmPassword");
    auto *clear = password.findChild<QCheckBox *>("clearPassword");
    auto *error = password.findChild<QLabel *>("passwordError");
    auto *buttons = password.findChild<QDialogButtonBox *>();
    if (!entry || !confirmation || !clear || !error || !buttons) return 5;
    password.show();
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    ok->click();
    if (calls || error->text().isEmpty()) return 6;
    entry->setText("secret");
    confirmation->setText("different");
    ok->click();
    if (calls || error->text().isEmpty()) return 7;
    confirmation->setText("secret");
    ok->click();
    if (calls != 1 || !password.isVisible() || error->text() != "Rejected by document") return 8;
    if (error->textFormat() != Qt::PlainText || !error->property("danger").toBool()) return 9;
    fail = false;
    ok->click();
    if (calls != 2 || applied != "secret" || password.result() != QDialog::Accepted) return 10;

    PasswordDialog removal([&](const QString &value) { applied = value; });
    removal.findChild<QCheckBox *>("clearPassword")->setChecked(true);
    if (removal.findChild<QLineEdit *>("newPassword")->isEnabled()
        || removal.findChild<QLineEdit *>("confirmPassword")->isEnabled()) return 11;
    removal.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    if (!applied.isEmpty() || removal.result() != QDialog::Accepted) return 12;

    PasswordDialog cancelled([&](const QString &) { ++calls; });
    cancelled.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
    if (calls != 2 || cancelled.result() != QDialog::Rejected) return 13;
    DigitalSignatureStatus signedStatus;
    signedStatus.fieldName = "<b>Signature A</b>";
    signedStatus.signedField = true;
    signedStatus.signer = "<b>Alice</b>";
    signedStatus.digestValid = true;
    signedStatus.integrity = DigitalSignatureStatus::Integrity::Intact;
    signedStatus.certificateValidation = DigitalSignatureStatus::CertificateValidation::Untrusted;
    signedStatus.digestStatus = "OK";
    signedStatus.certificateStatus = "Untrusted certificate";
    signedStatus.changedSinceSigning = true;
    signedStatus.certificateValidFrom = QDateTime::fromString("2024-01-02T03:04:05Z", Qt::ISODate);
    signedStatus.certificateValidUntil = QDateTime::fromString("2025-01-02T03:04:05Z", Qt::ISODate);
    signedStatus.signingTime = QDateTime::fromString("2024-06-07T08:09:10Z", Qt::ISODate);
    signedStatus.verificationTime = QDateTime::fromString("2026-01-01T00:00:00Z", Qt::ISODate);
    signedStatus.certificateProblems << "The signer certificate has expired.";
    signedStatus.certificateDescription = "Certificate dates are checked against the current UTC time.";
    DigitalSignatureStatus unsignedStatus;
    unsignedStatus.fieldName = "Signature B";
    DigitalSignatureStatus failedStatus;
    failedStatus.fieldName = "Signature C";
    failedStatus.error = "Provider failure";
    SignatureCheckDialog report({signedStatus, unsignedStatus, failedStatus}, "Test policy");
    report.show();
    app.processEvents();
    if (report.findChildren<QGroupBox *>().size() != 3) return 14;
    auto *rejectedCertificate = report.findChild<QLabel *>("signatureCertificateStatus1");
    if (!rejectedCertificate || !rejectedCertificate->text().startsWith("✗ ")
        || rejectedCertificate->styleSheet().isEmpty()) return 23;
    auto *firstToggle = report.findChild<QToolButton *>("signatureDetailsToggle1");
    auto *firstPanel = report.findChild<QWidget *>("signatureDetailPanel1");
    auto *firstDetails = report.findChild<QTextBrowser *>("signatureDetails1");
    auto *secondDetails = report.findChild<QTextBrowser *>("signatureDetails2");
    if (!firstToggle || !firstPanel || !firstDetails || secondDetails || firstToggle->isChecked()
        || !firstPanel->isHidden() || !firstDetails->isReadOnly()
        || report.findChild<QToolButton *>("signatureDetailsToggle2")
        || report.findChild<QToolButton *>("signatureDetailsToggle3")
        || report.findChild<QTextBrowser *>("signatureDetails3")) return 15;
    firstToggle->click();
    const QString detailText = firstDetails->toPlainText();
    if (firstPanel->isHidden() || firstToggle->arrowType() != Qt::DownArrow
        || !detailText.contains("current UTC time")
        || detailText.contains("Signature A") || detailText.contains("Alice")
        || !detailText.contains("Description:")
        || !detailText.contains("has expired") || !detailText.contains("Untrusted certificate")
        || detailText.contains("2024-") || detailText.contains("2025-")
        || detailText.contains("Digest status:") || detailText.contains("Certificate status:")) return 16;
    firstToggle->click();
    if (!firstPanel->isHidden() || firstToggle->arrowType() != Qt::RightArrow) return 17;
    QStringList summaries;
    for (auto *label : report.findChildren<QLabel *>()) {
        if (label->textFormat() != Qt::PlainText) return 18;
        summaries << label->text();
    }
    const QString text = summaries.join('\n');
    const auto displayedDate = [](const QDateTime &date) {
        return QLocale().toString(date.toUTC(), QStringLiteral("d MMM yyyy, HH:mm:ss 'UTC'"));
    };
    if (!text.contains("<b>Alice</b>") || text.contains("Untrusted certificate")
        || !text.contains("Later PDF revisions") || !text.contains("Unsigned signature field")
        || !text.contains("Provider failure") || !text.contains("Test policy")
        || !text.contains("Not trusted under the provider's policy") || text.contains("has expired")
        || !text.contains("Expired.")
        || !text.contains("Within certificate validity period (unverified time).")
        || !text.contains("Not checked by this verification provider.")
        || !text.contains("current and historical revocation are unknown")
        || text.contains("Description:")
        || !text.contains("Valid from:") || !text.contains("Valid until:")
        || !text.contains("Signed (claimed):")
        || !text.contains(displayedDate(signedStatus.certificateValidFrom))
        || !text.contains(displayedDate(signedStatus.certificateValidUntil))
        || !text.contains(displayedDate(signedStatus.signingTime))
        || !firstDetails->toPlainText().contains("not a verified timestamp")) return 19;
    report.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close)->click();
    if (report.isVisible()) return 20;
    SignatureCheckDialog empty({}, "Test policy");
    bool emptyMessage = false;
    for (auto *label : empty.findChildren<QLabel *>())
        emptyMessage |= label->text() == "No digital signature fields found.";
    if (!emptyMessage || !empty.findChildren<QGroupBox *>().isEmpty()) return 21;
    DigitalSignatureStatus acceptedStatus;
    acceptedStatus.signedField = true;
    acceptedStatus.certificateTrusted = true;
    acceptedStatus.certificateValidation = DigitalSignatureStatus::CertificateValidation::Accepted;
    acceptedStatus.integrity = DigitalSignatureStatus::Integrity::Intact;
    SignatureCheckDialog accepted({acceptedStatus}, "Test policy");
    bool acceptedMessage = false, missingDate = false;
    for (auto *label : accepted.findChildren<QLabel *>()) {
        acceptedMessage |= label->text().contains("Accepted by the verification provider.");
        missingDate |= label->text() == "Not available";
    }
    if (!acceptedMessage || !missingDate) return 22;
    auto *acceptedCertificate = accepted.findChild<QLabel *>("signatureCertificateStatus1");
    if (!acceptedCertificate || !acceptedCertificate->text().startsWith("✓ ")
        || acceptedCertificate->styleSheet().isEmpty()
        || acceptedCertificate->styleSheet() == rejectedCertificate->styleSheet()) return 24;
    const auto resultText = [](const DigitalSignatureStatus &status) {
        SignatureCheckDialog dialog({status}, "Test policy");
        auto *result = dialog.findChild<QLabel *>("signatureOverallStatus1");
        return result ? result->text() : QString();
    };
    if (!resultText(acceptedStatus).contains("Validity not established")
        || !resultText(unsignedStatus).startsWith("Unsigned")
        || !resultText(failedStatus).contains("Verification incomplete")) return 25;
    auto altered = acceptedStatus;
    altered.integrity = DigitalSignatureStatus::Integrity::Invalid;
    if (!resultText(altered).startsWith("✗ Invalid")) return 26;
    auto failedIntegrity = acceptedStatus;
    failedIntegrity.integrity = DigitalSignatureStatus::Integrity::Failed;
    if (!resultText(failedIntegrity).contains("Validity not established")) return 27;
    auto expiredButClaimedWithin = signedStatus;
    expiredButClaimedWithin.certificateValidation = DigitalSignatureStatus::CertificateValidation::Failed;
    expiredButClaimedWithin.certificateStatus = "Unknown error.";
    if (!resultText(expiredButClaimedWithin).contains("Validity not established")) return 28;
    auto current = acceptedStatus;
    const auto now = QDateTime::currentDateTimeUtc();
    current.certificateValidFrom = now.addDays(-1);
    current.certificateValidUntil = now.addDays(1);
    current.signingTime = now;
    current.verificationTime = now;
    if (!resultText(current).contains("Validity not established")) return 29;
    auto outside = current;
    outside.signingTime = now.addDays(-2);
    SignatureCheckDialog outsideReport({outside}, "Test policy");
    bool outsideMessage = false;
    for (auto *label : outsideReport.findChildren<QLabel *>())
        outsideMessage |= label->text().contains("Outside certificate validity period (unverified time).");
    if (!outsideMessage || !resultText(outside).contains("Validity not established")) return 30;
    auto malformed = current;
    malformed.certificateValidUntil = now.addDays(-2);
    SignatureCheckDialog malformedReport({malformed}, "Test policy");
    bool malformedMessage = false;
    for (auto *label : malformedReport.findChildren<QLabel *>())
        malformedMessage |= label->text().contains("Validity dates unavailable or malformed.");
    if (!malformedMessage) return 31;
    auto revised = current;
    revised.changedSinceSigning = true;
    if (!resultText(revised).contains("later revisions are not covered")) return 32;
    auto unknownChain = current;
    unknownChain.certificateValidation = DigitalSignatureStatus::CertificateValidation::Failed;
    if (!resultText(unknownChain).startsWith("⚠ Validity not established")) return 33;
    auto timestamped = current;
    timestamped.timestamp.status = SignatureTimestamp::Status::Verified;
    timestamped.timestamp.time = now;
    timestamped.timestamp.description = "Test timestamp diagnostic";
    SignatureCheckDialog timestampReport({timestamped}, "Test policy");
    auto *timestampLabel = timestampReport.findChild<QLabel *>("signatureTimestampStatus1");
    if (!timestampLabel || !timestampLabel->text().startsWith("✓ ")
        || !resultText(timestamped).contains("Validity not established")) return 34;
    bool timestampDate = false, certificateAtTimestamp = false;
    for (auto *label : timestampReport.findChildren<QLabel *>()) {
        timestampDate |= label->text() == "Timestamp time:";
        certificateAtTimestamp |= label->text() == "Certificate at timestamp:";
    }
    if (!timestampDate || !certificateAtTimestamp) return 35;
    timestamped.timestamp.status = SignatureTimestamp::Status::Invalid;
    timestamped.timestamp.time = {};
    SignatureCheckDialog invalidTimestamp({timestamped}, "Test policy");
    auto *invalidTimestampLabel = invalidTimestamp.findChild<QLabel *>("signatureTimestampStatus1");
    if (!invalidTimestampLabel || !invalidTimestampLabel->text().startsWith("✗ ")
        || !resultText(timestamped).contains("Validity not established")) return 36;
    timestamped.timestamp.status = SignatureTimestamp::Status::Absent;
    SignatureCheckDialog absentTimestamp({timestamped}, "Test policy");
    auto *absentTimestampLabel = absentTimestamp.findChild<QLabel *>("signatureTimestampStatus1");
    if (!absentTimestampLabel || !absentTimestampLabel->text().contains("No embedded")) return 37;
    auto onlineStatus = acceptedStatus;
    onlineStatus.revocationAttempted = true;
    SignatureRevocationCheck certificateCheck;
    certificateCheck.result.status = jpdf::revocation::Status::Good;
    certificateCheck.result.source = jpdf::revocation::Source::Ocsp;
    certificateCheck.result.thisUpdate = now.addSecs(-60);
    certificateCheck.result.nextUpdate = now.addSecs(3600);
    onlineStatus.revocationChecks = {certificateCheck};
    if (!resultText(onlineStatus).contains("Current revocation checks passed")
        || !resultText(onlineStatus).contains("validity not established")) return 38;
    auto revokedStatus = onlineStatus;
    revokedStatus.revocationChecks[0].timestampAuthority = true;
    revokedStatus.revocationChecks[0].result.status = jpdf::revocation::Status::Revoked;
    revokedStatus.revocationChecks[0].result.revokedAt = now.addDays(-1);
    revokedStatus.revocationChecks[0].result.invalidityDate = now.addDays(-2);
    revokedStatus.revocationChecks[0].result.revocationReason = 1;
    SignatureCheckDialog revokedReport({revokedStatus}, "Test policy");
    auto *revocationLabel = revokedReport.findChild<QLabel *>("signatureRevocationStatus1");
    auto *revocationDetails = revokedReport.findChild<QTextBrowser *>("signatureDetails1");
    if (!revocationLabel || !revocationLabel->text().startsWith("✗ ") || !revocationDetails
        || !revocationDetails->toPlainText().contains("TSA certificate")
        || !revocationDetails->toPlainText().contains("Key compromise")
        || !revocationDetails->toPlainText().contains("Effective invalidity date:")
        || !resultText(revokedStatus).contains("historical validity not established")) return 39;
    auto unknownRevocation = onlineStatus;
    unknownRevocation.revocationChecks[0].result.status = jpdf::revocation::Status::Unknown;
    if (!resultText(unknownRevocation).contains("Revocation incomplete")) return 40;
    auto clockFailure = onlineStatus;
    clockFailure.revocationChecks[0].result.status = jpdf::revocation::Status::CheckFailed;
    clockFailure.revocationChecks[0].result.clockError = true;
    if (!resultText(clockFailure).contains("System clock appears incorrect")
        || resultText(clockFailure).contains("Invalid")) return 45;
    // Opening/repopulating a report must not re-evaluate dates using the live clock.
    auto oldSnapshot = current;
    oldSnapshot.verificationTime = QDateTime::fromString("2000-01-02T00:00:00Z", Qt::ISODate);
    oldSnapshot.certificateValidFrom = oldSnapshot.verificationTime.addDays(-1);
    oldSnapshot.certificateValidUntil = oldSnapshot.verificationTime.addDays(1);
    SignatureCheckDialog snapshotReport({oldSnapshot}, "Test policy");
    bool snapshotWithin = false;
    for (auto *label : snapshotReport.findChildren<QLabel *>())
        snapshotWithin |= label->text() == "Within validity period.";
    if (!snapshotWithin) return 46;
    int onlineCalls = 0;
    bool completeOnline = false;
    SignatureCheckDialog consentReport({acceptedStatus}, "Test policy", nullptr,
        [&](QWidget *, QVector<DigitalSignatureStatus> &checks) {
            ++onlineCalls;
            checks[0] = onlineStatus;
            return completeOnline;
        });
    auto *onlineButton = consentReport.findChild<QPushButton *>("checkRevocationOnline");
    if (!onlineButton || !onlineButton->isEnabled()) return 41;
    const auto answer = [](QMessageBox::StandardButton button) {
        auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (message) message->button(button)->click();
    };
    QTimer::singleShot(0, [&] { answer(QMessageBox::No); });
    onlineButton->click();
    if (onlineCalls != 0) return 42;
    QTimer::singleShot(0, [&] { answer(QMessageBox::Yes); });
    onlineButton->click();
    if (onlineCalls != 1 || !consentReport.findChild<QLabel *>("signatureRevocationStatus1")->text().contains("Not checked")) return 43;
    completeOnline = true;
    QTimer::singleShot(0, [&] { answer(QMessageBox::Yes); });
    onlineButton->click();
    if (onlineCalls != 2 || !consentReport.findChild<QLabel *>("signatureRevocationStatus1")->text().startsWith("✓ ")
        || !consentReport.findChild<QLabel *>("signatureOverallStatus1")->text().contains("validity not established")) return 44;
    return 0;
}
