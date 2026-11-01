#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QBuffer>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstring>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QImage image(20, 10, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            image.setPixelColor(x, y, y < 5 ? Qt::red : Qt::green);
    QByteArray png;
    QBuffer encoded(&png);
    if (!encoded.open(QIODevice::WriteOnly) || !image.save(&encoded, "PNG")) return 2;

    const QString input = directory.filePath("input.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    // Leave a non-default CTM behind: added images must not inherit it.
    const char *drawing = "0 0 1 rg -1000 -1000 3000 3000 re f 1 0 0 1 1000 1000 cm\n";
    fz_buffer *contents = fz_new_buffer_from_copied_data(ctx,
        reinterpret_cast<const unsigned char *>(drawing), std::strlen(drawing));
    for (int i = 0; i < 4; ++i) {
        pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 300, 400}, i * 90, nullptr, contents);
        pdf_dict_put_rect(ctx, page, PDF_NAME(CropBox), {20, 30, 280, 370});
        if (i == 2) pdf_dict_put_real(ctx, page, PDF_NAME(UserUnit), 2);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    fz_drop_buffer(ctx, contents);
    pdf_save_document(ctx, doc, input.toUtf8().constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);

    PdfDocument document;
    document.open(input);
    if (!document.signatures(0).isEmpty()) return 3;
    DocumentAnnotations edits;
    for (int i = 0; i < 4; ++i)
        edits.signatures[i] = {{{60, 90, 120, 60}, png}, {{210, 210, 60, 30}, png}};
    const QString output = directory.filePath("output.pdf");
    document.saveSnapshot(output, edits);
    PdfDocument reopened;
    reopened.open(output);
    doc = pdf_open_document(ctx, output.toUtf8().constData());
    for (int i = 0; i < 4; ++i) {
        pdf_page *page = pdf_load_page(ctx, doc, i);
        const auto saved = reopened.signatures(i);
        if (pdf_first_annot(ctx, page) || saved.size() != 2
            || saved[0].id.isEmpty() || saved[1].id.isEmpty()
            || saved[0].id == saved[1].id || !saved[0].png.isEmpty()
            || saved[0].rect != edits.signatures[i][0].rect) return 4;
        pdf_drop_page(ctx, page);
        const QImage pixels = reopened.render(i);
        // Verify position, orientation, transparency, and a second placement.
        if (pixels.pixelColor(80, 100) != QColor(Qt::red)
            || pixels.pixelColor(80, 140) != QColor(Qt::green)
            || pixels.pixelColor(160, 100) != QColor(Qt::blue)
            || pixels.pixelColor(220, 215) != QColor(Qt::red)
            || pixels.pixelColor(220, 235) != QColor(Qt::green)) return 5;
    }
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);

    // Retaining IDs must not duplicate images, nor accept move/resize attempts.
    const QImage before = reopened.render(0);
    const auto saved = reopened.signatures(0);
    DocumentAnnotations retained;
    retained.signatures[0] = saved;
    retained.signatures[0][0].rect.translate(30, 40);
    retained.signatures[0][0].png = png;
    reopened.saveSnapshot(directory.filePath("again.pdf"), retained);
    if (reopened.render(0) != before || reopened.signatures(0).size() != 2
        || reopened.signatures(0)[0].id != saved[0].id
        || reopened.signatures(0)[0].rect != saved[0].rect) return 6;

    // Remove just one stream; a different signature and other pages stay intact.
    DocumentAnnotations removals;
    removals.signatures[0] = {saved[1]};
    const QImage otherPage = reopened.render(1);
    auto preview = reopened.snapshot(removals);
    if (preview->render(0).pixelColor(80, 100) != QColor(Qt::blue)
        || preview->render(0).pixelColor(220, 215) != QColor(Qt::red)
        || reopened.render(0) != before) return 7;
    reopened.saveSnapshot(directory.filePath("removed.pdf"), removals);
    if (reopened.signatures(0).size() != 1 || reopened.signatures(0)[0].id != saved[1].id
        || reopened.render(0) != preview->render(0) || reopened.render(1) != otherPage) return 8;

    // A new placement can be added alongside a retained saved image.
    removals.signatures[0] = reopened.signatures(0);
    removals.signatures[0].append({{60, 90, 120, 60}, png});
    reopened.saveSnapshot(directory.filePath("added.pdf"), removals);
    if (reopened.signatures(0).size() != 2 || reopened.signatures(0)[0].id != saved[1].id
        || reopened.signatures(0)[1].id == saved[0].id
        || reopened.render(0) != before) return 10;

    // Empty entries remove all tagged images, without deleting the blue page.
    removals.signatures[0] = {};
    reopened.saveSnapshot(directory.filePath("empty.pdf"), removals);
    PdfDocument empty;
    empty.open(directory.filePath("empty.pdf"));
    if (!empty.signatures(0).isEmpty()
        || empty.render(0).pixelColor(220, 215) != QColor(Qt::blue)) return 9;
    return 0;
}
