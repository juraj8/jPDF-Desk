#include "ui/signature_image.h"

#include <QCoreApplication>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QImage source(40, 20, QImage::Format_ARGB32);
    source.fill(Qt::white);
    if (!extractSignature(source).isNull() || !extractSignature({}).isNull()) return 2;
    const QColor ink(20, 40, 100);
    source.setPixelColor(20, 10, ink);
    const QString path = directory.filePath(QStringLiteral("signature.png"));
    if (!source.save(path)) return 3;
    const QImage loaded = loadSignatureSource(path);
    if (loaded != source) return 4;
    const QImage signature = extractSignature(loaded);
    if (signature.size() != QSize(7, 7) || signature.pixelColor(3, 3) != ink
        || signature.pixelColor(0, 0).alpha() != 0) return 5;
    QString error;
    if (!loadSignatureSource(directory.filePath(QStringLiteral("missing.png")), &error).isNull()
        || error.isEmpty()) return 6;
    return 0;
}
