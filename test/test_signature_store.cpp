#include "signature_store.h"
#include "ui/signature_manager.h"
#include "ui/sidebar.h"
#include "ui/window_style.h"

#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    try {
        QImage image(30, 10, QImage::Format_ARGB32);
        image.fill(Qt::black);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) return 2;
        SignatureStore images(SignatureStore::Kind::Image, directory.path());
        if (!images.entries().isEmpty() || !images.activeImage().isEmpty()) return 3;
        const auto first = images.importAsset(QStringLiteral("Signature / one"), png);
        const auto second = images.importAsset(QStringLiteral("Two"), png);
        // A fresh instance represents the next app session.
        SignatureStore reopened(SignatureStore::Kind::Image, directory.path());
        if (reopened.entries().size() != 2 || reopened.activeId() != second
            || reopened.activeImage() != png) return 4;
        reopened.select(first);
        reopened.rename(first, QStringLiteral("Renamed"));
        if (images.activeId() != first || images.entries()[0].name != QStringLiteral("Renamed")) return 5;
        SignatureManager manager(images);
        auto *list = manager.findChild<QListWidget *>(QStringLiteral("savedAssets"));
        auto *use = manager.findChild<QPushButton *>(QStringLiteral("useAsset"));
        if (!list || !use || list->count() != 2 || !use->isEnabled()) return 6;
        list->setCurrentRow(1);
        use->click();
        if (manager.result() != QDialog::Accepted || images.activeId() != second) return 7;
        const auto removedPath = images.activePath();
        images.remove(second);
        if (QFile::exists(removedPath) || !images.activeId().isEmpty() || images.entries().size() != 1) return 8;
        bool invalidRejected = false;
        try { images.importAsset(QStringLiteral("Bad"), QByteArray("not an image")); }
        catch (const std::exception &) { invalidRejected = true; }
        if (!invalidRejected || images.entries().size() != 1) return 9;

        SignatureStore certificates(SignatureStore::Kind::Certificate, directory.path());
        const QByteArray encryptedFixture("opaque PKCS12 bytes: validated by signing provider, not asset storage");
        const auto certificate = certificates.importAsset(QStringLiteral("Work identity"), encryptedFixture);
        SignatureStore certificatesAgain(SignatureStore::Kind::Certificate, directory.path());
        if (certificatesAgain.activeId() != certificate || certificatesAgain.entries().size() != 1
            || !certificatesAgain.activeImage().isEmpty()) return 10;
        QFile stored(certificatesAgain.activePath());
        if (!stored.open(QIODevice::ReadOnly) || stored.readAll() != encryptedFixture) return 11;
#ifdef Q_OS_UNIX
        if (stored.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)) return 12;
#endif
        stored.close();
        certificatesAgain.remove(certificate);
        if (!certificates.entries().isEmpty() || !certificates.activePath().isEmpty()) return 13;
        // Missing files do not leave an apparently usable active identity.
        images.select(first);
        if (!QFile::remove(images.activePath()) || !images.activeId().isEmpty()) return 14;
        Sidebar sidebar(QIcon{});
        sidebar.setStyleSheet(windowStyle());
        sidebar.setDocumentState(0, false, false, false);
        if (!sidebar.loadSignatureButton()->isEnabled() || !sidebar.manageCertificatesButton()->isEnabled()
            || sidebar.placeSignatureButton()->isEnabled()) return 15;
        if (sidebar.minimumSizeHint().height() > 800) {
            std::cerr << "Sidebar minimum height: " << sidebar.minimumSizeHint().height() << '\n';
            return 17;
        }
        if (!sidebar.addButton()->property("toolTile").toBool()
            || sidebar.addButton()->property("primary").toBool()) return 16;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 20;
    }
    return 0;
}
