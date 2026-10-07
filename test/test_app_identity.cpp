#include "app_identity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <stdexcept>

namespace {
bool writeAsset(const QString &root, const QString &library, const QByteArray &contents)
{
    const QString directory = QDir(root).filePath(library);
    if (!QDir().mkpath(directory)) return false;
    QFile file(QDir(directory).filePath("asset"));
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString oldRoot = directory.filePath("old");
    const QString newRoot = directory.filePath("new");
    QSettings oldSettings(directory.filePath("old.ini"), QSettings::IniFormat);
    QSettings newSettings(directory.filePath("new.ini"), QSettings::IniFormat);
    oldSettings.setValue("appearance/theme", "dark");
    oldSettings.setValue("existing", "old");
    newSettings.setValue("existing", "new");
    if (!writeAsset(oldRoot, "signature-images", "image") ||
        !writeAsset(oldRoot, "signing-certificates", "private key")) return 2;
    QFile oldKey(QDir(oldRoot).filePath("signing-certificates/asset"));
    if (!oldKey.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return 3;
    const auto permissions = oldKey.permissions();

    migrateApplicationData(oldRoot, newRoot, oldSettings, newSettings);
    if (newSettings.value("appearance/theme") != "dark" ||
        newSettings.value("existing") != "new" || oldSettings.value("existing") != "old") return 4;
    QFile migratedKey(QDir(newRoot).filePath("signing-certificates/asset"));
    if (!migratedKey.open(QIODevice::ReadOnly) || migratedKey.readAll() != "private key" ||
        migratedKey.permissions() != permissions || QFile::exists(oldKey.fileName())) return 5;
    if (!QFile::exists(QDir(newRoot).filePath("signature-images/asset"))) return 6;
    // Repeat startup, and never replace a library created under the new name.
    migrateApplicationData(oldRoot, newRoot, oldSettings, newSettings);
    if (!writeAsset(oldRoot, "signature-images", "old image")) return 7;
    migrateApplicationData(oldRoot, newRoot, oldSettings, newSettings);
    QFile image(QDir(newRoot).filePath("signature-images/asset"));
    if (!image.open(QIODevice::ReadOnly) || image.readAll() != "image" ||
        !QFile::exists(QDir(oldRoot).filePath("signature-images/asset"))) return 8;

    // Failure must leave the original private-key library intact.
    QFile blocked(directory.filePath("blocked"));
    if (!blocked.open(QIODevice::WriteOnly)) return 9;
    blocked.close();
    if (!writeAsset(oldRoot, "signing-certificates", "retained key")) return 10;
    try {
        migrateApplicationData(oldRoot, blocked.fileName(), oldSettings, newSettings);
        return 11;
    } catch (const std::runtime_error &) {
        if (!QFile::exists(QDir(oldRoot).filePath("signing-certificates/asset"))) return 12;
    }
    return 0;
}
