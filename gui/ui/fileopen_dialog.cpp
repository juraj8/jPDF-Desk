#include "ui/fileopen_dialog.h"
#include "app_info.h"
#include "jpdf_desk/document/pdf_file_diagnostics.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QUrl>

void showFileOpenWarning(QWidget *parent, const QString &path, const QString &error)
{
    QMessageBox dialog(QMessageBox::Warning, QObject::tr("FileOpen-protected PDF"),
        QObject::tr("This PDF uses FileOpen DRM protection, which jPDF Desk does not support. "
                    "A normal PDF password will not unlock it.\n\n"
                    "Follow the document provider's instructions, usually Adobe Acrobat Reader "
                    "with the FileOpen plug-in and the required authorization, or ask the "
                    "provider for a PDF without FileOpen protection.\n\n"
                    "If you already have a compatible reader installed, you can choose it below."),
        QMessageBox::Cancel, parent);
    dialog.setDetailedText(pdfFileDiagnostics(path) + QStringLiteral("\n\n")
                           + QObject::tr("Original error: %1").arg(error));
    auto *open = dialog.addButton(QObject::tr("Choose external reader…"), QMessageBox::ActionRole);
    const QUrl donationUrl(QString::fromUtf8(AppInfo::donate));
    QPushButton *donate = nullptr;
    if (donationUrl.isValid() && !donationUrl.isEmpty()
        && (donationUrl.scheme() == QStringLiteral("https")
            || donationUrl.scheme() == QStringLiteral("http")
            || donationUrl.scheme() == QStringLiteral("mailto"))) {
        donate = dialog.addButton(QObject::tr("Donate"), QMessageBox::ActionRole);
        donate->setObjectName(QStringLiteral("donateButton"));
        donate->setToolTip(QObject::tr("Support jPDF Desk development (does not unlock this PDF)."));
    }
    dialog.setDefaultButton(QMessageBox::Cancel);
    dialog.exec();
    if (donate && dialog.clickedButton() == donate) {
        if (!QDesktopServices::openUrl(donationUrl)) {
            QMessageBox::warning(parent, QObject::tr("Cannot open donation link"),
                                 QObject::tr("Open this donation link manually:\n%1")
                                     .arg(donationUrl.toString()));
        }
        return;
    }
    if (dialog.clickedButton() != open) return;

    // Do not use the default PDF association: it may point back to this app.
    const QString reader = QFileDialog::getOpenFileName(parent, QObject::tr("Choose external PDF reader"));
    if (reader.isEmpty()) return;
    const QString readerPath = QFileInfo(reader).canonicalFilePath();
    bool isThisApp = readerPath == QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
#ifdef Q_OS_MACOS
    // A native macOS chooser may return the application bundle instead of its executable.
    isThisApp = isThisApp || readerPath == QFileInfo(QCoreApplication::applicationDirPath()
                                                   + QStringLiteral("/../..")).canonicalFilePath();
#endif
    if (isThisApp) {
        QMessageBox::warning(parent, QObject::tr("Cannot open external reader"),
                             QObject::tr("Choose a different application that supports FileOpen-protected PDFs."));
        return;
    }
    const QString document = QFileInfo(path).absoluteFilePath();
#ifdef Q_OS_MACOS
    const bool started = reader.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)
        ? QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-a"), reader, document})
        : QProcess::startDetached(reader, {document});
#else
    const bool started = QProcess::startDetached(reader, {document});
#endif
    if (!started) {
        QMessageBox::warning(parent, QObject::tr("Cannot open external reader"),
                             QObject::tr("The selected reader could not be started. "
                                         "Open the PDF from your compatible reader manually."));
    }
}
