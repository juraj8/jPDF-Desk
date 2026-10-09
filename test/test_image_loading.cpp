#include "jpdf_desk/image/image_loading.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    try {
        QString error;
        if (!loadSourceImage(directory.filePath("missing.png"), &error).isNull() || error.isEmpty()) return 12;
        QImage source(10, 5, QImage::Format_ARGB32);
        source.fill(QColor(20, 40, 80, 128));
        for (const auto &format : {"PNG", "BMP"}) {
            const QString path = directory.filePath(QStringLiteral("source.%1").arg(QString::fromLatin1(format)));
            if (!source.save(path, format)) return 13;
            const QImage decoded = loadSourceImage(path);
            if (decoded.size() != source.size() || decoded.pixelColor(0, 0).red() != 20) return 14;
            if (QString::fromLatin1(format) == QStringLiteral("PNG") && decoded.pixelColor(0, 0).alpha() != 128) return 15;
        }
        const QString largePath = directory.filePath("large.png");
        QFile large(largePath);
        if (!large.open(QIODevice::WriteOnly) || !large.resize(32 * 1024 * 1024 + 1)) return 16;
        large.close();
        error.clear();
        if (!loadSourceImage(largePath, &error).isNull() || !error.contains("too large")) return 17;
        const QString brokenPath = directory.filePath("broken.jpg");
        QFile broken(brokenPath);
        const QByteArray bytes = QByteArray::fromHex("ffd8ffe00010") + "truncated JPEG";
        if (!broken.open(QIODevice::WriteOnly) || broken.write(bytes) != bytes.size()) return 2;
        broken.close();
        std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
            fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), fz_drop_context);
        fz_context *ctx = context.get();
        if (!ctx) return 3;
        auto pix = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return fz_new_pixmap(ctx, fz_device_rgb(ctx), 40, 20, nullptr, 0);
        }), fz_drop_pixmap);
        mupdf::call(ctx, [&]() noexcept { fz_clear_pixmap_with_value(ctx, pix.get(), 255); });
        const QString validPath = directory.filePath("valid.jpg");
        const QByteArray name = QFile::encodeName(validPath);
        mupdf::call(ctx, [&]() noexcept { fz_save_pixmap_as_jpeg(ctx, pix.get(), name.constData(), 90); });
        // Patch only the JPEG frame dimensions: exercise the size rejection
        // after image acquisition without allocating a huge pixel fixture.
        QFile valid(validPath);
        if (!valid.open(QIODevice::ReadOnly)) return 8;
        QByteArray oversized = valid.readAll();
        valid.close();
        auto frame = oversized.indexOf(QByteArray::fromHex("ffc0"));
        if (frame < 0) frame = oversized.indexOf(QByteArray::fromHex("ffc2"));
        if (frame < 0 || frame + 8 >= oversized.size()) return 9;
        oversized[frame + 5] = oversized[frame + 7] = char(0x13);
        oversized[frame + 6] = oversized[frame + 8] = char(0x88); // 5000 x 5000
        const QString oversizedPath = directory.filePath("oversized.jpg");
        QFile huge(oversizedPath);
        if (!huge.open(QIODevice::WriteOnly) || huge.write(oversized) != oversized.size()) return 10;
        huge.close();
        for (int iteration = 0; iteration < 20; ++iteration) {
            QString error;
            if (!loadSourceImage(brokenPath, &error).isNull() || error.isEmpty()) return 4;
            if (!loadSourceImage(brokenPath).isNull()) return 5;
            error.clear();
            if (!loadSourceImage(oversizedPath, &error).isNull() ||
                !error.contains("16 megapixels")) return 11;
            const QImage decoded = loadSourceImage(validPath);
            if (decoded.size() != QSize(40, 20) || qGray(decoded.pixel(0, 0)) < 240) return 6;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 7;
    }
    return 0;
}
