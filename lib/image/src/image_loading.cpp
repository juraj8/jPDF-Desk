#include "jpdf_desk/image/image_loading.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <QFile>
#include <QImageReader>
#include <QObject>

QImage loadSourceImage(const QString &path, QString *error)
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
    std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
        fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), fz_drop_context);
    fz_context *ctx = context.get();
    if (!ctx) {
        if (error) *error = QObject::tr("Cannot initialize image decoder.");
        return {};
    }
    try {
        auto buffer = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return fz_new_buffer_from_copied_data(ctx,
                reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.size());
        }), fz_drop_buffer);
        auto image = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return fz_new_image_from_buffer(ctx, buffer.get());
        }), fz_drop_image);
        if (qint64(image->w) * image->h > 16'000'000) {
            if (error) *error = QObject::tr("Choose an image under 16 megapixels.");
            return {};
        }
        auto pix = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return fz_get_unscaled_pixmap_from_image(ctx, image.get());
        }), fz_drop_pixmap);
        auto rgb = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return fz_convert_pixmap(ctx, pix.get(), fz_device_rgb(ctx), nullptr, nullptr,
                                     fz_default_color_params, 0);
        }), fz_drop_pixmap);
        // No Qt object or allocation crosses a MuPDF longjmp boundary.
        return QImage(fz_pixmap_samples(ctx, rgb.get()), fz_pixmap_width(ctx, rgb.get()),
                      fz_pixmap_height(ctx, rgb.get()), fz_pixmap_stride(ctx, rgb.get()),
                      QImage::Format_RGB888).copy();
    } catch (const std::exception &exception) {
        if (error) *error = QString::fromUtf8(exception.what());
        return {};
    }
}
