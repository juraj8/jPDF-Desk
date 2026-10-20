#include "main_window.h"
#include "ui/signature_check_dialog.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QLabel>
#include <QTextBrowser>
#include <QToolButton>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <mupdf/pdf.h>

namespace {
bool hasExpectedTitle(const QMessageBox &message, const QString &title)
{
    // QMessageBox::setWindowTitle() is deliberately a no-op on macOS,
    // including when using the offscreen platform plugin.
#ifdef Q_OS_MACOS
    Q_UNUSED(message);
    Q_UNUSED(title);
    return true;
#else
    return message.windowTitle() == title;
#endif
}

// Exercise the UI's typed-error route without changing the process/OS clock.
class ClockFailureProvider final : public VerificationProvider {
public:
    pdf_pkcs7_verifier *createVerifier(fz_context *) const override
    {
        jpdf::crypto::ValidationTime::atSystemTime(
            jpdf::crypto::detail::validationClockBounds().build).requireValidClock();
        return nullptr;
    }
    QString trustDescription() const override { return QStringLiteral("Test trust policy"); }
};
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const auto filename = directory.filePath("clock-test.pdf");
    const auto encoded = QFile::encodeName(filename);
    {
        std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
            fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), fz_drop_context);
        if (!context) return 2;
        auto *ctx = context.get();
        auto doc = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_create_document(ctx);
        }), pdf_drop_document);
        auto page = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_add_page(ctx, doc.get(), {0, 0, 300, 400}, 0, nullptr, nullptr);
        }), pdf_drop_obj);
        mupdf::call(ctx, [&]() noexcept {
            pdf_insert_page(ctx, doc.get(), -1, page.get());
            pdf_save_document(ctx, doc.get(), encoded.constData(), &pdf_default_write_options);
        });
    }
    MainWindow window(nullptr, {{}, std::make_shared<ClockFailureProvider>()});
    window.openDocument(filename);
    auto *button = window.findChild<QPushButton *>("verifySignaturesButton");
    if (!button || !button->isEnabled()) return 3;
    bool appropriateMessage = false;
    QTimer::singleShot(0, [&] {
        auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!message) return;
        appropriateMessage = hasExpectedTitle(*message, QStringLiteral("System clock appears incorrect"))
            && message->icon() == QMessageBox::Warning
            && message->text().contains("Signature validity has not been determined")
            && message->text().contains("Correct the system date and time")
            && !message->text().contains("Invalid signature");
        message->accept();
    });
    button->click();
    if (!appropriateMessage) return 4;

    DigitalSignatureStatus blocked;
    blocked.signedField = true;
    blocked.integrity = DigitalSignatureStatus::Integrity::Intact;
    blocked.digestValid = true;
    blocked.clockError = "Test clock is too early";
    DigitalSignatureStatus corrected = blocked;
    corrected.clockError.clear();
    corrected.verificationTime = QDateTime::currentDateTimeUtc();
    corrected.certificateValidation = CertificateChainValidation::Accepted;
    corrected.certificateTrusted = true;
    // Rechecking must discard old online evidence, even if a callback carries it over.
    corrected.revocationAttempted = true;
    SignatureRevocationCheck previous;
    previous.result.status = jpdf::revocation::Status::Good;
    corrected.revocationChecks = {previous};
    int rechecks = 0, onlineChecks = 0;
    bool fail = true;
    QVector<DigitalSignatureStatus> refreshed{corrected};
    SignatureCheckDialog report({blocked}, "Test policy", nullptr,
        [&](QWidget *, QVector<DigitalSignatureStatus> &) { ++onlineChecks; return true; },
        [&] {
            ++rechecks;
            if (fail) throw std::runtime_error("Test recheck failed");
            return refreshed;
        });
    auto *details = report.findChild<QWidget *>("signatureDetailPanel1");
    auto *toggle = report.findChild<QToolButton *>("signatureDetailsToggle1");
    auto *notes = report.findChild<QTextBrowser *>("signatureDetails1");
    if (!details || !details->isHidden() || !toggle || !notes
        || !notes->toPlainText().contains("Diagnostics and validation notes")) return 11;
    toggle->click();
    if (details->isHidden() || toggle->arrowType() != Qt::DownArrow) return 12;
    toggle->click();
    if (!details->isHidden() || toggle->arrowType() != Qt::RightArrow) return 13;
    const auto label = [&](const char *name) { return report.findChild<QLabel *>(name); };
    auto *retry = report.findChild<QPushButton *>("recheckSignatures");
    auto *online = report.findChild<QPushButton *>("checkRevocationOnline");
    if (!retry || !retry->isEnabled() || !online || online->isEnabled()
        || !label("signatureOverallStatus1")->text().contains("System clock appears incorrect")
        || !label("signatureIntegrityStatus1")->text().contains("Signed bytes are intact")
        || !label("signatureCertificateStatus1")->text().contains("Not checked")) return 5;
    online->click();
    if (onlineChecks != 0) return 6;
    bool retryError = false;
    QTimer::singleShot(0, [&] {
        auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!message) return;
        retryError = hasExpectedTitle(*message, QStringLiteral("Cannot recheck signatures"))
            && message->icon() == QMessageBox::Warning
            && message->text() == QStringLiteral("Test recheck failed");
        message->accept();
    });
    retry->click();
    if (!retryError || rechecks != 1 || online->isEnabled()
        || !label("signatureOverallStatus1")->text().contains("System clock appears incorrect")) return 7;
    fail = false;
    retry->click();
    if (rechecks != 2 || onlineChecks != 0 || !online->isEnabled()
        || label("signatureOverallStatus1")->text().contains("System clock")
        || !label("signatureCertificateStatus1")->text().contains("Accepted")
        || !label("signatureRevocationStatus1")->text().contains("Not checked")) return 8;
    refreshed.clear();
    retry->click();
    if (rechecks != 3 || online->isEnabled() || label("signatureOverallStatus1")) return 9;

    blocked.integrity = DigitalSignatureStatus::Integrity::Invalid;
    blocked.certificateDescription = QStringLiteral("Issuer: <Test CA>\nSubject: A & B");
    SignatureCheckDialog altered({blocked}, "Test policy");
    if (!altered.findChild<QLabel *>("signatureOverallStatus1")->text().startsWith("✗ Invalid")
        || !altered.findChild<QLabel *>("signatureIntegrityStatus1")->text().contains("altered")) return 10;
    auto *certificateNotes = altered.findChild<QTextBrowser *>("signatureDetails1");
    if (!certificateNotes || !certificateNotes->toPlainText().contains("Certificate information")
        || !certificateNotes->toPlainText().contains(blocked.certificateDescription)) return 14;
    return 0;
}
