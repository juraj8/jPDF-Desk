#include "app_identity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <stdexcept>

void migrateApplicationData(const QString &legacyRoot, const QString &currentRoot,
                            QSettings &legacySettings, QSettings &currentSettings)
{
    // Keep current choices if both versions have been used. The legacy settings
    // remain untouched so a failed migration can be retried safely.
    for (const auto &key : legacySettings.allKeys()) {
        if (!currentSettings.contains(key))
            currentSettings.setValue(key, legacySettings.value(key));
    }
    currentSettings.sync();
    if (currentSettings.status() != QSettings::NoError)
        throw std::runtime_error("Cannot migrate application preferences.");

    for (const auto &name : {QStringLiteral("signature-images"),
                             QStringLiteral("signing-certificates")}) {
        const QString source = QDir(legacyRoot).filePath(name);
        const QString destination = QDir(currentRoot).filePath(name);
        if (!QFileInfo::exists(source) || QFileInfo::exists(destination)) continue;
        // Rename rather than copy private keys; preserve existing permissions.
        if (!QDir().mkpath(currentRoot) || !QDir().rename(source, destination))
            throw std::runtime_error("Cannot move the saved signature/certificate library. "
                                     "The original library has not been deleted.");
    }
}

void initializeApplicationIdentity()
{
    // The old name is used only to locate data from earlier releases.
    const QString legacyName = QStringLiteral("PDF Filler");
    QCoreApplication::setOrganizationName(legacyName);
    QCoreApplication::setApplicationName(legacyName);
    const QString legacyRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    const QString currentName = QStringLiteral("jPDF Desk");
    QCoreApplication::setOrganizationName(currentName);
    QCoreApplication::setApplicationName(currentName);
    QGuiApplication::setApplicationDisplayName(currentName);
    const QString currentRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QSettings legacySettings(legacyName, legacyName);
    QSettings currentSettings(currentName, currentName);
    migrateApplicationData(legacyRoot, currentRoot, legacySettings, currentSettings);
}
