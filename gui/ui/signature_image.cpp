#include "ui/signature_image.h"
#include "jpdf_desk/image/image_loading.h"

#include <QtGlobal>
#include <algorithm>

QImage loadSignatureSource(const QString &path, QString *error)
{
    return loadSourceImage(path, error);
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
