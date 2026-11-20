#include "update_check.h"
#include "app_info.h"
#include "ui/dialog_helpers.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <utility>

QString newerStableReleaseTag(const QByteArray &response, const QVersionNumber &current, QString &error)
{
    error.clear();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QStringLiteral("Invalid GitHub release JSON: %1").arg(parseError.errorString());
        return {};
    }
    if (!document.isObject()) {
        error = QStringLiteral("Expected a GitHub release object");
        return {};
    }
    const auto release = document.object();
    if (release.value(QStringLiteral("draft")).toBool() ||
        release.value(QStringLiteral("prerelease")).toBool()) return {};

    const auto tag = release.value(QStringLiteral("tag_name")).toString();
    // Match the stable tag format used by the release workflow.
    static const QRegularExpression stableTag(QStringLiteral(R"(\Av[0-9]+\.[0-9]+\.[0-9]+\z)"));
    qsizetype suffixIndex = 0;
    const auto latest = QVersionNumber::fromString(tag.mid(1), &suffixIndex);
    if (!stableTag.match(tag).hasMatch() || suffixIndex != tag.size() - 1) {
        error = QStringLiteral("Missing or invalid GitHub release tag");
        return {};
    }
    return latest > current ? tag : QString{};
}

QUrl releasePageUrl(const QString &tag)
{
    return QUrl(QStringLiteral("https://github.com/juraj8/jPDF-Desk/releases/tag/") + tag);
}

void checkForUpdates(QObject *context, std::function<void(const QString &tag)> onAvailable)
{
    const auto version = QString::fromUtf8(AppInfo::version);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/juraj8/jPDF-Desk/releases/latest")));
    request.setRawHeader("User-Agent", QByteArray("jPDF-Desk/") + version.toUtf8());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(5000);

    auto *network = new QNetworkAccessManager(context);
    auto *reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, context,
                     [network, reply, version, onAvailable = std::move(onAvailable)] {
        network->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning().noquote() << "Update check failed:" << reply->errorString();
            return;
        }
        QString error;
        const auto tag = newerStableReleaseTag(reply->readAll(), QVersionNumber::fromString(version), error);
        if (!error.isEmpty()) qWarning().noquote() << "Update check failed:" << error;
        if (!tag.isEmpty()) onAvailable(tag);
    });
}

void showUpdateNotification(QWidget *parent)
{
    checkForUpdates(parent, [parent](const QString &tag) {
        auto *notice = new QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        notice->setWindowTitle(QCoreApplication::translate("UpdateCheck", "Update available"));
        notice->setAttribute(Qt::WA_DeleteOnClose);
        notice->setMinimumWidth(440);

        auto *layout = new QVBoxLayout(notice);
        layout->setContentsMargins(24, 22, 24, 22);
        layout->setSpacing(12);
        auto *heading = dialogHint(QCoreApplication::translate("UpdateCheck",
            "jPDF Desk %1 is ready to download.").arg(tag.mid(1)), notice);
        auto font = heading->font();
        font.setBold(true);
        heading->setFont(font);
        layout->addWidget(heading);
        layout->addWidget(dialogHint(QCoreApplication::translate("UpdateCheck",
            "You’re on version %1. See what’s new and get the latest release on GitHub.")
                .arg(QString::fromUtf8(AppInfo::version)), notice));

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, notice);
        auto *releaseButton = buttons->addButton(
            QCoreApplication::translate("UpdateCheck", "See what’s new"), QDialogButtonBox::AcceptRole);
        releaseButton->setDefault(true);
        QObject::connect(buttons, &QDialogButtonBox::rejected, notice, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, notice, [notice, url = releasePageUrl(tag)] {
            QDesktopServices::openUrl(url);
            notice->accept();
        });
        layout->addWidget(buttons);
        notice->show();
    });
}
