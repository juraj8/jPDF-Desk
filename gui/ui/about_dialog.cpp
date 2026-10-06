#include "ui/about_dialog.h"
#include "ui/dialog_helpers.h"
#include "app_info.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(const QIcon &icon, QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("About jPDF Desk"));
    setWindowIcon(icon);
    resize(540, 420);
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QHBoxLayout;
    auto *logo = new QLabel(this);
    logo->setPixmap(icon.pixmap(64, 64));
    heading->addWidget(logo);
    auto *title = new QLabel(tr("jPDF Desk"), this);
    title->setObjectName(QStringLiteral("windowHeading"));
    heading->addWidget(title, 1);
    layout->addLayout(heading);

    auto *description = new QLabel(QString::fromUtf8(AppInfo::description), this);
    description->setObjectName(QStringLiteral("aboutDescription"));
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *form = new QFormLayout;
    layout->addLayout(form);
    const auto addDetail = [this, form](const QString &caption, const QString &value, const QString &name) {
        auto *label = new QLabel(value, this);
        label->setObjectName(name);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        form->addRow(caption, label);
        return label;
    };
    addDetail(tr("Project:"), QString::fromUtf8(AppInfo::name), QStringLiteral("aboutProject"));
    addDetail(tr("Version:"), QString::fromUtf8(AppInfo::version), QStringLiteral("aboutVersion"));
    addDetail(tr("Author:"), QString::fromUtf8(AppInfo::author), QStringLiteral("aboutAuthor"));
    const QString homepage = QString::fromUtf8(AppInfo::homepage);
    const QUrl url(homepage);
    auto *page = addDetail(tr("Project page:"), tr("Not configured"), QStringLiteral("aboutHomepage"));
    if (url.isValid() && !url.host().isEmpty() &&
        (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"))) {
        page->setTextFormat(Qt::RichText);
        page->setText(QStringLiteral("<a href=\"%1\">%2</a>")
            .arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped(), homepage.toHtmlEscaped()));
        page->setTextInteractionFlags(Qt::TextBrowserInteraction);
        page->setOpenExternalLinks(true);
    }
    addDetail(tr("License:"), tr("MIT (application code)"), QStringLiteral("aboutLicense"));
    addDetail(tr("Platform:"), QString::fromUtf8(AppInfo::platform), QStringLiteral("aboutPlatform"));
    addDetail(tr("Qt version:"), QString::fromLatin1(qVersion()), QStringLiteral("aboutQtVersion"));
    auto *details = new QLabel(tr("View, fill, annotate, print, sign and verify PDFs; edit metadata and manage password protection. Built with Qt and MuPDF; third-party components have their own licenses."), this);
    details->setWordWrap(true);
    layout->addWidget(details);
    auto *licenseNotice = new QLabel(tr("Original jPDF Desk source code is licensed under MIT. Third-party dependencies retain their respective licenses. Builds incorporating open-source MuPDF are subject to AGPLv3-or-later requirements."), this);
    licenseNotice->setObjectName(QStringLiteral("aboutLicenseNotice"));
    licenseNotice->setWordWrap(true);
    licenseNotice->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(licenseNotice);
    layout->addStretch();
    auto *buttons = dialogButtons(this, QDialogButtonBox::Close);
    layout->addWidget(buttons);
}
