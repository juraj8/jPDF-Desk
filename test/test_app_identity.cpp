#include "app_identity.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    initializeApplicationIdentity();
    if (QCoreApplication::organizationName() != "jPDF Desk" ||
        QCoreApplication::applicationName() != "jPDF Desk" ||
        QGuiApplication::applicationDisplayName() != "jPDF Desk") return 1;

    const QString expectedRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath("jPDF-Desk");
    if (applicationDataRoot() != expectedRoot) return 2;

    // Asset storage must not depend on the display/settings identity.
    QCoreApplication::setOrganizationName("Other organization");
    QCoreApplication::setApplicationName("Other application");
    if (applicationDataRoot() != expectedRoot) return 3;
    return 0;
}
