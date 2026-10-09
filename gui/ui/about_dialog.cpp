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
    resize(666, 400);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *heading = new QHBoxLayout;
    auto *logo = new QLabel(this);
    logo->setPixmap(icon.pixmap(64, 64));
    heading->addWidget(logo);
    auto *title = new QLabel(QStringLiteral("%1 <small>%2</small>")
        .arg(QString::fromUtf8(AppInfo::name).toHtmlEscaped(),
             QString::fromUtf8(AppInfo::version).toHtmlEscaped()), this);
    title->setTextFormat(Qt::RichText);
    title->setObjectName(QStringLiteral("windowHeading"));
    heading->addWidget(title, 1);
    layout->addLayout(heading);

    auto *description = new QLabel(QString::fromUtf8(AppInfo::description), this);
    description->setObjectName(QStringLiteral("aboutDescription"));
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
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
    const auto addLink = [&addDetail](const QString &caption, const QString &target,
                                      const QString &text, const QString &name) {
        auto *label = addDetail(caption, tr("Not configured"), name);
        const QUrl link(target);
        if (!link.isValid() || link.isEmpty()) return;
        if (link.scheme() != QStringLiteral("https") && link.scheme() != QStringLiteral("http")
            && link.scheme() != QStringLiteral("mailto")) return;
        label->setTextFormat(Qt::RichText);
        label->setText(QStringLiteral("<a href=\"%1\">%2</a>")
            .arg(link.toString(QUrl::FullyEncoded).toHtmlEscaped(), text.toHtmlEscaped()));
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);
    };
    addDetail(tr("Author:"), QStringLiteral("%1 <%2>")
        .arg(QString::fromUtf8(AppInfo::author), QString::fromUtf8(AppInfo::authorEmail)),
        QStringLiteral("aboutAuthor"));
    addLink(tr("Issues:"), QString::fromUtf8(AppInfo::issues), tr("Report an issue on GitHub"),
            QStringLiteral("aboutIssues"));
    const QString donationUrl = QString::fromUtf8(AppInfo::donate);
    const QString donationText = donationUrl == QStringLiteral("https://github.com/sponsors/juraj8")
        ? tr("Sponsor on GitHub")
        : (QUrl(donationUrl).scheme() == QStringLiteral("mailto")
            ? tr("Contact the author about donating") : tr("Donate"));
    addLink(tr("Support development:"), donationUrl, donationText,
            QStringLiteral("aboutDonate"));
    addDetail(tr("License:"), tr("MIT (application code)"), QStringLiteral("aboutLicense"));
    auto *licenseNotice = new QLabel(tr("Original jPDF Desk source code is licensed under MIT. Third-party dependencies retain their respective licenses. Builds incorporating open-source MuPDF are subject to AGPLv3-or-later requirements."), this);
    licenseNotice->setObjectName(QStringLiteral("aboutLicenseNotice"));
    licenseNotice->setWordWrap(true);
    licenseNotice->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(licenseNotice);
    layout->addStretch();
    auto *buttons = dialogButtons(this, QDialogButtonBox::Close);
    layout->addWidget(buttons);
    // Account for wrapped text rather than relying only on the initial height.
    resize(width(), qMax(height(), layout->totalHeightForWidth(width())));
}
