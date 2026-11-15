#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <limits>

namespace {
void makePdf(const QString &path, bool large = false)
{
    auto *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    auto *doc = pdf_create_document(ctx);
    auto *object = pdf_add_page(ctx, doc, large ? fz_rect{0, 0, 100000, 10000}
                                               : fz_rect{0, 0, 100, 100}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, doc, -1, object);
    pdf_drop_obj(ctx, object);
    if (!large) {
        auto *page = pdf_load_page(ctx, doc, 0);
        const auto square = [&](fz_rect rect, const char *author, const float *color, int flags) {
            auto *annot = pdf_create_annot(ctx, page, PDF_ANNOT_SQUARE);
            pdf_set_annot_rect(ctx, annot, rect);
            pdf_set_annot_author(ctx, annot, author);
            pdf_set_annot_color(ctx, annot, 3, color);
            pdf_set_annot_interior_color(ctx, annot, 3, color);
            pdf_set_annot_flags(ctx, annot, flags);
            pdf_drop_annot(ctx, annot);
        };
        const float red[] = {1, 0, 0}, blue[] = {0, 0, 1}, green[] = {0, 1, 0};
        square({10, 10, 40, 40}, "Another app", red, PDF_ANNOT_IS_PRINT);
        // An owned but non-editable annotation must still be visible.
        square({60, 10, 90, 40}, "jPDF Desk", blue, PDF_ANNOT_IS_PRINT);
        square({10, 60, 40, 90}, "Another app", green, PDF_ANNOT_IS_HIDDEN);
        pdf_update_page(ctx, page);
        pdf_drop_page(ctx, page);
    }
    pdf_save_document(ctx, doc, path.toUtf8().constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString input = dir.filePath("annotations.pdf");
    makePdf(input);
    PdfDocument doc;
    doc.open(input);
    doc.saveSnapshot(dir.filePath("edited.pdf"), {{}, {{0, {{OptionMark::Check, {75, 120}}}}}, {}});
    const QImage view = doc.render(0), printed = doc.renderForPrint(0, 108);
    for (const auto &image : {view, printed}) {
        if (image.pixelColor(30, 30) != QColor(Qt::red) ||
            image.pixelColor(110, 30) != QColor(Qt::blue) ||
            image.pixelColor(30, 110) != QColor(Qt::white)) return 2;
    }
    // Editable marks are scene overlays in the viewer, but included in print.
    int viewInk = 0, printInk = 0;
    for (int y = 105; y < 135; ++y)
        for (int x = 60; x < 90; ++x) {
            viewInk += view.pixelColor(x, y).red() < 128;
            printInk += printed.pixelColor(x, y).red() < 128;
        }
    if (viewInk != 0 || printInk < 10) return 3;
    for (qreal invalid : {qreal(0), qreal(-1), std::numeric_limits<qreal>::infinity(),
                         std::numeric_limits<qreal>::quiet_NaN()}) {
        try { doc.render(0, invalid); return 4; }
        catch (const std::runtime_error &) {}
    }
    const QString large = dir.filePath("large.pdf");
    makePdf(large, true);
    doc.open(large);
    const QImage bounded = doc.render(0, 100);
    if (bounded.isNull() || bounded.width() > 8192 || bounded.height() > 8192 ||
        qint64(bounded.width()) * bounded.height() > 17 * 1024 * 1024) return 5;
    try { doc.renderForPrint(0, 300); return 6; }
    catch (const std::runtime_error &) {}
    return 0;
}
