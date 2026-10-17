#include "ui/signature_check_dialog.h"
#include "ui/dialog_helpers.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <exception>
#include <utility>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QLabel *selectableText(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    return label;
}

enum class Tone { Neutral, Good, Bad, Uncertain };

void statusStyle(QLabel *label, Tone tone)
{
    if (tone == Tone::Neutral) return;
    const bool dark = label->palette().color(QPalette::Window).lightness() < 128;
    const QString color = tone == Tone::Good ? (dark ? QStringLiteral("#81c784") : QStringLiteral("#2e7d32"))
        : tone == Tone::Bad ? (dark ? QStringLiteral("#ef9a9a") : QStringLiteral("#c62828"))
        : (dark ? QStringLiteral("#ffcc80") : QStringLiteral("#8a5200"));
    label->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));
}

enum class RevocationOutcome { NotChecked, NotApplicable, Good, Revoked, Incomplete, ClockError };

RevocationOutcome revocationOutcome(const DigitalSignatureStatus &signature)
{
    if (!signature.revocationAttempted) return RevocationOutcome::NotChecked;
    for (const auto &check : signature.revocationChecks)
        if (check.result.clockError) return RevocationOutcome::ClockError;
    bool complete = !signature.revocationChecks.isEmpty(), applicable = false;
    for (const auto &check : signature.revocationChecks) {
        if (check.trustAnchorOnly) continue;
        applicable = true;
        if (check.result.status == jpdf::revocation::Status::Revoked) return RevocationOutcome::Revoked;
        complete &= check.result.status == jpdf::revocation::Status::Good;
    }
    if (!applicable && !signature.revocationChecks.isEmpty()) return RevocationOutcome::NotApplicable;
    return complete ? RevocationOutcome::Good : RevocationOutcome::Incomplete;
}

QString revocationReason(int reason)
{
    switch (reason) {
    case 0: return QObject::tr("Unspecified");
    case 1: return QObject::tr("Key compromise");
    case 2: return QObject::tr("CA compromise");
    case 3: return QObject::tr("Affiliation changed");
    case 4: return QObject::tr("Superseded");
    case 5: return QObject::tr("Cessation of operation");
    case 6: return QObject::tr("Certificate hold");
    case 8: return QObject::tr("Removed from CRL");
    case 9: return QObject::tr("Privilege withdrawn");
    case 10: return QObject::tr("AA compromise");
    default: return QObject::tr("Not specified");
    }
}

enum class DateCheck { Unknown, Within, Before, After };

DateCheck checkDates(const DigitalSignatureStatus &signature, const QDateTime &time)
{
    if (!time.isValid() || !signature.certificateValidFrom.isValid()
        || !signature.certificateValidUntil.isValid()
        || signature.certificateValidFrom > signature.certificateValidUntil)
        return DateCheck::Unknown;
    if (time < signature.certificateValidFrom) return DateCheck::Before;
    if (time > signature.certificateValidUntil) return DateCheck::After;
    return DateCheck::Within;
}
}

SignatureCheckDialog::SignatureCheckDialog(const QVector<DigitalSignatureStatus> &signatures,
                                         const QString &trustDescription, QWidget *parent, OnlineCheck onlineCheck,
                                         Recheck recheck)
    : QDialog(parent), signatures_(signatures), onlineAvailable_(bool(onlineCheck))
{
    setObjectName(QStringLiteral("signatureCheckDialog"));
    setWindowTitle(tr("Digital signature check"));
    resize(720, 600);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(dialogHint(tr("Checks the saved PDF only; unsaved edits are excluded. "
        "Integrity and certificate trust are separate checks. Trust policy: %1")
        .arg(trustDescription), this));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    content_ = new QWidget(scroll);
    cards_ = new QVBoxLayout(content_);
    scroll->setWidget(content_);
    layout->addWidget(scroll);
    online_ = new QPushButton(tr("Check revocation online…"), this);
    online_->setObjectName(QStringLiteral("checkRevocationOnline"));
    online_->setToolTip(onlineCheck ? tr("Optional: contact issuer services after confirmation. No historical validity claim.")
                                  : tr("Online revocation is unavailable in this build."));
    auto *actions = new QHBoxLayout;
    actions->addWidget(online_);
    connect(online_, &QPushButton::clicked, this, [this, onlineCheck] {
        if (QMessageBox::question(this, tr("Online revocation privacy"),
            tr("Contact certificate issuer and timestamp-authority services for the signer and TSA chains? "
               "These requests can disclose certificate identifiers and that this document was checked. "
               "Checks use current evidence, not proof of historical validity. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        try {
            auto checked = signatures_;
            if (onlineCheck(this, checked)) {
                signatures_ = std::move(checked);
                refreshCards();
            }
        } catch (const std::exception &error) {
            QMessageBox::warning(this, tr("Revocation check failed"), QString::fromUtf8(error.what()));
        }
    });
    auto *retry = new QPushButton(tr("Recheck saved PDF"), this);
    retry->setObjectName(QStringLiteral("recheckSignatures"));
    retry->setEnabled(bool(recheck));
    retry->setToolTip(tr("After correcting the system clock, recheck the saved PDF with a fresh time snapshot. "
                        "Previous online results are cleared; this does not contact issuer services."));
    actions->addWidget(retry);
    actions->addStretch();
    layout->addLayout(actions);
    connect(retry, &QPushButton::clicked, this, [this, recheck] {
        try {
            auto checked = recheck();
            for (auto &signature : checked) {
                signature.revocationAttempted = false;
                signature.revocationChecks.clear();
                signature.revocationTime = {};
            }
            signatures_ = std::move(checked);
            refreshCards();
        } catch (const std::exception &error) {
            QMessageBox::warning(this, tr("Cannot recheck signatures"), QString::fromUtf8(error.what()));
        }
    });
    refreshCards();
    layout->addWidget(dialogButtons(this, QDialogButtonBox::Close));
}

void SignatureCheckDialog::refreshCards()
{
    while (auto *item = cards_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    populateCards();
    bool eligible = false;
    for (const auto &signature : signatures_)
        eligible |= signature.signedField && signature.error.isEmpty() && signature.clockError.isEmpty();
    online_->setEnabled(onlineAvailable_ && eligible);
}

void SignatureCheckDialog::populateCards()
{
    auto *content = content_;
    auto *cards = cards_;
    const auto &signatures = signatures_;
    const auto dateText = [&](const QDateTime &date) {
        return date.isValid()
            ? QLocale().toString(date.toUTC(), QStringLiteral("d MMM yyyy, HH:mm:ss 'UTC'"))
            : tr("Not available");
    };
    if (signatures.isEmpty())
        cards->addWidget(selectableText(tr("No digital signature fields found."), content));
    for (int i = 0; i < signatures.size(); ++i) {
        const auto &signature = signatures[i];
        auto *card = new QGroupBox(tr("Signature %1 — %2").arg(i + 1).arg(signature.fieldName), content);
        card->setObjectName(QStringLiteral("signatureCard%1").arg(i + 1));
        auto *cardLayout = new QVBoxLayout(card);
        auto *summary = new QFormLayout;
        summary->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        summary->setRowWrapPolicy(QFormLayout::WrapLongRows);
        summary->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        cardLayout->addLayout(summary);
        auto *detailPanel = new QWidget(card);
        detailPanel->setObjectName(QStringLiteral("signatureDetailPanel%1").arg(i + 1));
        auto *detailLayout = new QVBoxLayout(detailPanel);
        detailLayout->setContentsMargins(0, 8, 0, 0);
        auto *dates = new QFormLayout;
        dates->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        dates->setRowWrapPolicy(QFormLayout::WrapLongRows);
        dates->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        auto *datesHeading = new QLabel(tr("Dates and time evidence"), detailPanel);
        datesHeading->setTextFormat(Qt::PlainText);
        auto headingFont = datesHeading->font();
        headingFont.setBold(true);
        datesHeading->setFont(headingFont);
        detailLayout->addWidget(datesHeading);
        detailLayout->addLayout(dates);
        const auto detailRow = [&](const QString &name, const QString &value) {
            auto *label = selectableText(name, detailPanel);
            label->setWordWrap(false);
            dates->addRow(label, selectableText(value, detailPanel));
        };
        const auto row = [&](const QString &name, const QString &value) {
            auto *label = selectableText(name, card);
            label->setWordWrap(false);
            auto *text = selectableText(value, card);
            summary->addRow(label, text);
            return text;
        };
        QStringList details;
        using Integrity = DigitalSignatureStatus::Integrity;
        using Validation = DigitalSignatureStatus::CertificateValidation;
        using Timestamp = SignatureTimestamp::Status;
        const bool timestampVerified = signature.clockError.isEmpty()
            && signature.timestamp.status == Timestamp::Verified && signature.timestamp.time.isValid();
        const auto revocation = revocationOutcome(signature);
        QString decision = tr("⚠ Validity not established");
        Tone decisionTone = Tone::Uncertain;
        if (signature.integrity == Integrity::Invalid) {
            decision = tr("✗ Invalid — signed bytes have been altered");
            decisionTone = Tone::Bad;
        } else if (!signature.clockError.isEmpty()) {
            decision = tr("⚠ System clock appears incorrect — trust/time validation not established");
        } else if (revocation == RevocationOutcome::ClockError) {
            decision = tr("⚠ System clock appears incorrect — revocation not checked; validity not established");
        } else if (!signature.error.isEmpty()) {
            decision = tr("⚠ Verification incomplete — validity not established");
        } else if (!signature.signedField) {
            decision = tr("Unsigned — no signature to validate");
            decisionTone = Tone::Neutral;
        } else if (signature.certificateValidation == Validation::Untrusted) {
            decision = tr("✗ Not trusted under the provider's policy");
            decisionTone = Tone::Bad;
        } else {
            if (revocation == RevocationOutcome::Revoked)
                decision = tr("⚠ Revocation reported — historical validity not established");
            else if (revocation == RevocationOutcome::Incomplete)
                decision = tr("⚠ Revocation incomplete — validity not established");
            else if (revocation == RevocationOutcome::Good) {
                const bool currentChecksPassed = signature.integrity == Integrity::Intact
                    && signature.certificateValidation == Validation::Accepted
                    && checkDates(signature, signature.revocationTime) == DateCheck::Within
                    && timestampVerified && checkDates(signature, signature.timestamp.time) == DateCheck::Within
                    && signature.coverageChecked && !signature.changedSinceSigning;
                decision = currentChecksPassed ? tr("⚠ Current checks passed — historical validity not established")
                                               : tr("⚠ Current revocation checks passed — validity not established");
            }
            if (signature.changedSinceSigning && revocation != RevocationOutcome::Revoked)
                decision += tr(" — later revisions are not covered");
            details << (signature.revocationAttempted
                ? tr("Online results describe current evidence only. Historical validity still requires appropriate time, archived revocation, and chain evidence.")
                : timestampVerified
                ? tr("A verified timestamp provides time evidence, but overall validity still requires signer-chain and historical revocation evidence.")
                : tr("Overall validity cannot be established without verified signing-time and revocation evidence. "
                    "Passing integrity and certificate-chain checks alone is insufficient."));
        }
        auto *overall = row(tr("Overall result:"), decision);
        overall->setObjectName(QStringLiteral("signatureOverallStatus%1").arg(i + 1));
        statusStyle(overall, decisionTone);
        if (!signature.clockError.isEmpty()) {
            row(tr("Clock:"), tr("Correct the system date and time, then select Recheck saved PDF. "
                                "Integrity is independent of certificate trust."));
            details << signature.clockError;
        }
        if (!signature.error.isEmpty())
            row(tr("Verification:"), tr("Could not verify: %1").arg(signature.error));
        if (!signature.signedField) {
            if (signature.error.isEmpty()) row(tr("Status:"), tr("Unsigned signature field."));
        } else {
            row(tr("Identity:"), signature.signer.isEmpty() ? tr("Unknown") : signature.signer);
            const bool intact = signature.integrity == Integrity::Intact;
            auto *integrity = row(tr("Integrity:"), intact ? tr("✓ Signed bytes are intact.")
                : signature.integrity == Integrity::Invalid ? tr("✗ Signed bytes have been altered.")
                : tr("⚠ Could not establish integrity."));
            integrity->setObjectName(QStringLiteral("signatureIntegrityStatus%1").arg(i + 1));
            statusStyle(integrity, intact ? Tone::Good
                : signature.integrity == Integrity::Invalid ? Tone::Bad : Tone::Uncertain);
            if (!intact && !signature.digestStatus.isEmpty())
                details << tr("Integrity diagnostic: %1").arg(signature.digestStatus);
            const auto validation = signature.clockError.isEmpty()
                ? signature.certificateValidation : Validation::NotChecked;
            auto *certificateStatus = row(tr("Certificate chain:"), !signature.clockError.isEmpty()
                ? tr("⚠ Not checked — system clock appears incorrect.") : validation == Validation::Accepted
                ? tr("✓ Accepted by the verification provider.")
                : validation == Validation::Untrusted ? tr("✗ Not trusted by the verification provider.")
                : tr("⚠ Chain validation not established."));
            certificateStatus->setObjectName(QStringLiteral("signatureCertificateStatus%1").arg(i + 1));
            statusStyle(certificateStatus, validation == Validation::Accepted ? Tone::Good
                : validation == Validation::Untrusted ? Tone::Bad : Tone::Uncertain);
            detailRow(tr("Offline checked:"), dateText(signature.verificationTime));
            const auto currentDates = signature.clockError.isEmpty()
                ? checkDates(signature, signature.verificationTime) : DateCheck::Unknown;
            detailRow(tr("Certificate at offline check:"), !signature.clockError.isEmpty()
                ? tr("Not checked — system clock appears incorrect.") : currentDates == DateCheck::Within ? tr("Within validity period.")
                : currentDates == DateCheck::After ? tr("Expired.")
                : currentDates == DateCheck::Before ? tr("Not yet valid.") : tr("Validity dates unavailable or malformed."));
            const auto claimedDates = checkDates(signature, signature.signingTime);
            detailRow(tr("At claimed signing:"), claimedDates == DateCheck::Within ? tr("Within certificate validity period (unverified time).")
                : claimedDates == DateCheck::Unknown ? tr("Cannot compare: signing time or certificate dates unavailable.")
                : tr("Outside certificate validity period (unverified time)."));
            auto *timestampStatus = row(tr("Signature timestamp:"), !signature.clockError.isEmpty()
                ? tr("⚠ Not checked — system clock appears incorrect.") : timestampVerified
                ? tr("✓ RFC 3161 timestamp verified.")
                : signature.timestamp.status == Timestamp::Absent ? tr("No embedded signature timestamp found.")
                : signature.timestamp.status == Timestamp::Invalid ? tr("✗ Invalid timestamp token or signature imprint.")
                : signature.timestamp.status == Timestamp::Failed ? tr("⚠ Timestamp validation not established.")
                : tr("Not checked by this verification provider."));
            timestampStatus->setObjectName(QStringLiteral("signatureTimestampStatus%1").arg(i + 1));
            statusStyle(timestampStatus, timestampVerified ? Tone::Good
                : signature.timestamp.status == Timestamp::Invalid ? Tone::Bad : Tone::Uncertain);
            if (!signature.timestamp.description.isEmpty())
                details << tr("Timestamp diagnostic: %1").arg(signature.timestamp.description);
            auto *revocationLabel = row(tr("Revocation:"), !signature.clockError.isEmpty()
                ? tr("Not checked — recheck the saved PDF after correcting the clock.") : revocation == RevocationOutcome::Good
                ? tr("✓ Good — current evidence for all checked signer/TSA certificates.")
                : revocation == RevocationOutcome::Revoked ? tr("✗ Revocation reported; see certificate dates and reasons in Details.")
                : revocation == RevocationOutcome::ClockError ? tr("⚠ System clock appears incorrect. Correct the date and time, then retry.")
                : revocation == RevocationOutcome::Incomplete ? tr("⚠ Incomplete — unknown, failed, unavailable, or skipped checks.")
                : revocation == RevocationOutcome::NotApplicable ? tr("Trusted anchors only — no non-root certificates to query.")
                : tr("Not checked — current and historical revocation are unknown."));
            revocationLabel->setObjectName(QStringLiteral("signatureRevocationStatus%1").arg(i + 1));
            statusStyle(revocationLabel, revocation == RevocationOutcome::Good ? Tone::Good
                : revocation == RevocationOutcome::Revoked ? Tone::Bad
                : revocation == RevocationOutcome::NotApplicable ? Tone::Neutral : Tone::Uncertain);
            if (signature.revocationAttempted)
                detailRow(tr("Revocation checked:"), dateText(signature.revocationTime));
            for (const auto &check : signature.revocationChecks) {
                const auto &result = check.result;
                using Status = jpdf::revocation::Status;
                const QString status = check.trustAnchorOnly ? tr("Trusted anchor (not queried)")
                    : result.status == Status::Good ? tr("Good (current evidence)")
                    : result.status == Status::Revoked ? tr("Revoked")
                    : result.status == Status::Unknown ? tr("Unknown")
                    : result.clockError ? tr("System clock appears incorrect")
                    : result.status == Status::CheckFailed ? tr("Check failed") : tr("Not checked");
                details << tr("%1 certificate %2 — %3: %4")
                    .arg(check.timestampAuthority ? tr("TSA") : tr("Signer")).arg(check.depth + 1)
                    .arg(check.subject.isEmpty() ? tr("Unavailable") : check.subject).arg(status);
                if (result.source != jpdf::revocation::Source::None)
                    details << tr("Evidence: %1").arg(result.source == jpdf::revocation::Source::Ocsp ? tr("OCSP") : tr("CRL"));
                if (result.thisUpdate.isValid()) details << tr("This update: %1").arg(dateText(result.thisUpdate));
                if (result.nextUpdate.isValid()) details << tr("Next update: %1").arg(dateText(result.nextUpdate));
                if (result.revokedAt.isValid()) details << tr("Revoked at: %1").arg(dateText(result.revokedAt));
                if (result.invalidityDate.isValid()) details << tr("Effective invalidity date: %1").arg(dateText(result.invalidityDate));
                if (result.status == Status::Revoked)
                    details << tr("Revocation reason: %1 (code %2)").arg(revocationReason(result.revocationReason)).arg(result.revocationReason);
                details << result.diagnostics;
            }
            if (signature.revocationAttempted)
                details << tr("Current Good evidence does not prove historical validity. Revocation after a trusted timestamp does not automatically invalidate an earlier signature; key/CA compromise and effective invalidity dates require historical evidence and policy.");
            if (currentDates == DateCheck::After || currentDates == DateCheck::Before)
                details << tr("The certificate's validity at the offline check does not establish whether this older signature was valid when created. "
                    "A claimed signing date is not reliable historical evidence.");
            QStringList reasons = signature.certificateProblems;
            if (validation != Validation::Accepted) {
                if (!signature.certificateStatus.isEmpty())
                    reasons.prepend(tr("Certificate-chain diagnostic: %1").arg(signature.certificateStatus));
                if (signature.certificateStatus == QStringLiteral("Self-signed certificate."))
                    reasons << tr("The signer certificate is self-signed and is not accepted by the provider's trust store.");
                else if (signature.certificateStatus == QStringLiteral("Self-signed certificate in chain."))
                    reasons << tr("The certificate chain contains a self-signed certificate that is not accepted by the provider's trust store.");
                else if (signature.certificateStatus == QStringLiteral("Unknown error."))
                    reasons << tr("The verification provider did not expose a more specific chain-validation reason.");
            }
            if (!reasons.isEmpty())
                details << tr("Description:\n%1").arg(reasons.join(QLatin1Char('\n')));
            detailRow(tr("Valid from:"), dateText(signature.certificateValidFrom));
            detailRow(tr("Valid until:"), dateText(signature.certificateValidUntil));
            detailRow(tr("Signed (claimed):"), dateText(signature.signingTime));
            if (timestampVerified) {
                detailRow(tr("Timestamp time:"), dateText(signature.timestamp.time));
                const auto timestampDates = checkDates(signature, signature.timestamp.time);
                detailRow(tr("Certificate at timestamp:"), timestampDates == DateCheck::Within
                    ? tr("Within certificate validity period.")
                    : timestampDates == DateCheck::Unknown ? tr("Certificate validity dates unavailable.")
                    : tr("Outside certificate validity period; historical validity remains unestablished."));
            }
            details << tr("The PDF signing date is claimed by the signer, not a verified timestamp.");
            row(tr("Document coverage:"), signature.changedSinceSigning
                ? tr("Later PDF revisions exist; this signature does not cover the current document in full.")
                : signature.coverageChecked ? tr("No later PDF revisions detected.")
                : tr("Document coverage not established."));
        }
        cards->addWidget(card);
        if (details.isEmpty() && dates->rowCount() == 0 && signature.certificateDescription.isEmpty()) {
            delete detailPanel;
            continue;
        }
        datesHeading->setVisible(dates->rowCount() > 0);
        auto *toggle = new QToolButton(card);
        toggle->setObjectName(QStringLiteral("signatureDetailsToggle%1").arg(i + 1));
        toggle->setText(tr("Show details"));
        toggle->setCheckable(true);
        toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toggle->setArrowType(Qt::RightArrow);
        cardLayout->addWidget(toggle, 0, Qt::AlignLeft);
        auto *raw = new QTextBrowser(detailPanel);
        raw->setObjectName(QStringLiteral("signatureDetails%1").arg(i + 1));
        raw->setOpenLinks(false);
        QString html = QStringLiteral("<h3>%1</h3>").arg(tr("Diagnostics and validation notes").toHtmlEscaped());
        for (const auto &detail : details) {
            if (detail.trimmed().isEmpty()) continue;
            html += QStringLiteral("<p>%1</p>").arg(detail.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
        }
        if (!signature.certificateDescription.isEmpty()) {
            html += QStringLiteral("<h3>%1</h3><p>%2</p>")
                .arg(tr("Certificate information").toHtmlEscaped(),
                     signature.certificateDescription.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
        }
        raw->setHtml(html);
        raw->setMinimumHeight(200);
        raw->setMaximumHeight(300);
        raw->setVisible(!details.isEmpty() || !signature.certificateDescription.isEmpty());
        detailLayout->addWidget(raw);
        detailPanel->hide();
        cardLayout->addWidget(detailPanel);
        connect(toggle, &QToolButton::toggled, detailPanel, [toggle, detailPanel](bool expanded) {
            toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            toggle->setText(expanded ? tr("Hide details") : tr("Show details"));
            detailPanel->setVisible(expanded);
        });
    }
    cards->addStretch();
}
