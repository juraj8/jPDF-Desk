#include "app_identity.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>

QString applicationDataRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("jPDF-Desk"));
}

void initializeApplicationIdentity()
{
    const QString name = QStringLiteral("jPDF Desk");
    QCoreApplication::setOrganizationName(name);
    QCoreApplication::setApplicationName(name);
    QGuiApplication::setApplicationDisplayName(name);
}
