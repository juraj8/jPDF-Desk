#include "ui/signature_image.h"

#include <QtGlobal>
#include <QFile>
#include <QImageReader>
#include <QObject>
#include <mupdf/fitz.h>
#include <algorithm>

QImage loadSignatureSource(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    if (file.size() > 32 * 1024 * 1024) {
        if (error) *error = QObject::tr("Image file is too large.");
        return {};
    }
    // PNG and BMP are handled by Qt itself. Decode JPEG with MuPDF, which is
    // already linked and has a different libjpeg ABI from Qt's static plugin.
    if (file.peek(2) != QByteArray::fromHex("ffd8")) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage image = reader.read();
        if (error && image.isNull()) *error = reader.errorString();
        return image;
    }
    const QByteArray bytes = file.readAll();
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        if (error) *error = QObject::tr("Cannot initialize image decoder.");
        return {};
    }
    fz_buffer *buffer = nullptr;
    fz_image *image = nullptr;
    fz_pixmap *pix = nullptr;
    fz_pixmap *rgb = nullptr;
    bool failed = false, tooLarge = false;
    fz_try(ctx) {
        buffer = fz_new_buffer_from_copied_data(ctx,
            reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.size());
        image = fz_new_image_from_buffer(ctx, buffer);
        if (qint64(image->w) * image->h > 16'000'000)
            tooLarge = true;
        else {
            pix = fz_get_unscaled_pixmap_from_image(ctx, image);
            rgb = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), nullptr, nullptr,
                                    fz_default_color_params, 0);
        }
    }
    fz_catch(ctx) { failed = true; }
    if (error && failed) *error = QString::fromUtf8(fz_caught_message(ctx));
    if (error && tooLarge) *error = QObject::tr("Choose an image under 16 megapixels.");
    QImage result;
    if (rgb && !failed) {
        QImage view(fz_pixmap_samples(ctx, rgb), fz_pixmap_width(ctx, rgb),
                    fz_pixmap_height(ctx, rgb), fz_pixmap_stride(ctx, rgb), QImage::Format_RGB888);
        result = view.copy();
    }
    fz_drop_pixmap(ctx, rgb);
    fz_drop_pixmap(ctx, pix);
    fz_drop_image(ctx, image);
    fz_drop_buffer(ctx, buffer);
    fz_drop_context(ctx);
    return result;
}

QImage extractSignature(const QImage &source)
{
    if (source.isNull() || qint64(source.width()) * source.height() > 16'000'000)
        return {};
    // Use a light corner as the paper color. Preserve the RGB of colored ink.
    const QColor corner = source.pixelColor(0, 0);
    const int background = std::max({corner.red(), corner.green(), corner.blue(), 240});
    QImage ink(source.size(), QImage::Format_ARGB32);
    ink.fill(Qt::transparent);
    int left = source.width(), top = source.height(), right = -1, bottom = -1;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const QColor pixel = source.pixelColor(x, y);
            const int darkest = std::min({pixel.red(), pixel.green(), pixel.blue()});
            const int alpha = pixel.alpha() * qBound(0, (background - darkest - 12) * 4, 255) / 255;
            if (alpha < 24) continue;
            ink.setPixelColor(x, y, QColor(pixel.red(), pixel.green(), pixel.blue(), alpha));
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
        }
    }
    if (right < left || bottom < top) return {};
    const QRect bounds(QPoint(qMax(0, left - 3), qMax(0, top - 3)),
                       QPoint(qMin(source.width() - 1, right + 3),
                              qMin(source.height() - 1, bottom + 3)));
    QImage cropped = ink.copy(bounds);
    if (cropped.width() > 1400 || cropped.height() > 1400)
        cropped = cropped.scaled(1400, 1400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return cropped;
}
