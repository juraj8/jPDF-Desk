#include "pdf_filler/document/pdf_document.h"
#include "pdf_filler/printing/pdf_printing.h"
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QBuffer>
#include <exception>
#include <QFile>
#include <QPageRanges>
#include <QPrinter>
#include <QTemporaryDir>

namespace {
bool hasInk(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qGray(image.pixel(x, y)) < 128) return true;
    return false;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString input = dir.filePath("input.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    for (int i = 0; i < 3; ++i) {
        pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, nullptr, nullptr);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    pdf_save_document(ctx, doc, QFile::encodeName(input).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
    PdfDocument original;
    original.open(input);
    PdfDocument snapshot;
    snapshot.open(input);
    QImage signature(40, 20, QImage::Format_ARGB32);
    signature.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !signature.save(&buffer, "PNG")) return 11;
    const DocumentAnnotations annotations{
        {{0, {{{50, 80, 200, 50}, "Printed text", 18}}}},
        {{1, {{OptionMark::Check, {100, 100}}}}, {2, {{OptionMark::Cross, {100, 100}}}}},
        {{2, {{{200, 200, 80, 40}, png}}}}};
    snapshot.save(dir.filePath("snapshot.pdf"), annotations.fields, annotations.marks,
                  annotations.signatures);
    if (original.path() != input || !original.fields(0).isEmpty() ||
        hasInk(original.renderForPrint(0, 72)) || !hasInk(snapshot.renderForPrint(0, 72)) ||
        !hasInk(snapshot.renderForPrint(1, 72)) || !hasInk(snapshot.renderForPrint(2, 72))) return 2;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setResolution(150);
    if (selectedPrintPages(printer, 3, 1) != QVector<int>({0, 1, 2})) return 3;
    printer.setPrintRange(QPrinter::CurrentPage);
    if (selectedPrintPages(printer, 3, 1) != QVector<int>({1})) return 4;
    printer.setOutputFileName(dir.filePath("current.pdf"));
    printDocumentSnapshot(original, annotations, printer, 1);
    if (original.path() != input || !original.fields(0).isEmpty()
        || !original.marks(1).isEmpty() || !original.signatures(2).isEmpty()) return 13;
    PdfDocument result;
    result.open(printer.outputFileName());
    if (result.pageCount() != 1 || !hasInk(result.render(0))) return 5;

    printer.setPrintRange(QPrinter::PageRange);
    QPageRanges ranges;
    ranges.addPage(1);
    ranges.addPage(3);
    printer.setPageRanges(ranges);
    if (selectedPrintPages(printer, 3, 0) != QVector<int>({0, 2})) return 6;
    printer.setPageOrder(QPrinter::LastPageFirst);
    if (selectedPrintPages(printer, 3, 0) != QVector<int>({2, 0})) return 7;
    printer.setOutputFileName(dir.filePath("range.pdf"));
    printDocument(snapshot, printer, 0);
    result.open(printer.outputFileName());
    if (result.pageCount() != 2 || !hasInk(result.render(0)) || !hasInk(result.render(1))) return 8;

    printer.setPageRanges({});
    printer.setPrintRange(QPrinter::AllPages);
    printer.setPageOrder(QPrinter::FirstPageFirst);
    printer.setOutputFileName(dir.filePath("all.pdf"));
    printDocument(snapshot, printer, 0);
    result.open(printer.outputFileName());
    if (result.pageCount() != 3) return 9;
    if (!printer.supportsMultipleCopies()) {
        printer.setCopyCount(2);
        printer.setCollateCopies(true);
        printer.setOutputFileName(dir.filePath("copies.pdf"));
        printDocument(snapshot, printer, 0);
        result.open(printer.outputFileName());
        if (result.pageCount() != 6) return 12;
        printer.setCopyCount(1);
    }
    PrintOptions layout;
    layout.pagesPerSheet = 2;
    printer.setOutputFileName(dir.filePath("two-up.pdf"));
    printDocument(snapshot, printer, 0, layout);
    result.open(printer.outputFileName());
    if (result.pageCount() != 2 || !hasInk(result.render(0)) || !hasInk(result.render(1))) return 16;
    if (!printer.supportsMultipleCopies()) {
        printer.setCopyCount(2);
        printer.setCollateCopies(false);
        printer.setOutputFileName(dir.filePath("two-up-copies.pdf"));
        printDocument(snapshot, printer, 0, layout);
        result.open(printer.outputFileName());
        if (result.pageCount() != 4) return 17;
        printer.setCopyCount(1);
    }
    layout.subset = PrintOptions::PageSubset::Odd;
    if (selectedPrintPages(printer, 3, 0, layout) != QVector<int>({0, 2})) return 18;
    layout.subset = PrintOptions::PageSubset::Even;
    if (selectedPrintPages(printer, 3, 0, layout) != QVector<int>({1})) return 19;
    printer.setOutputFileName(dir.filePath("even.pdf"));
    printDocument(snapshot, printer, 0, layout);
    result.open(printer.outputFileName());
    if (result.pageCount() != 1) return 20;
    layout.pagesPerSheet = 3;
    try {
        printDocument(snapshot, printer, 0, layout);
        return 21;
    } catch (const std::exception &) {}

    printer.setPrintRange(QPrinter::CurrentPage);
    // Grayscale must affect raster content, not just the printer job settings.
    for (const auto mode : {QPrinter::Color, QPrinter::GrayScale}) {
        printer.setColorMode(mode);
        printer.setOutputFileName(dir.filePath(mode == QPrinter::Color ? "colour.pdf" : "gray.pdf"));
        printDocument(snapshot, printer, 2);
        result.open(printer.outputFileName());
        const QImage image = result.renderForPrint(0, 72).convertToFormat(QImage::Format_RGB32);
        bool hasColour = false;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y);
                if (qAbs(qRed(pixel) - qGreen(pixel)) > 10 || qAbs(qGreen(pixel) - qBlue(pixel)) > 10)
                    hasColour = true;
            }
        if (hasColour != (mode == QPrinter::Color)) return 15;
    }
    try {
        printDocument(snapshot, printer, 99);
        return 10;
    } catch (const std::exception &) {}

    // Empty snapshot entries must remove saved annotations from the printout,
    // without deleting them from the source document.
    printer.setOutputFileName(dir.filePath("deleted.pdf"));
    printDocumentSnapshot(snapshot, {{{0, {}}}, {{1, {}}, {2, {}}}, {{2, {}}}}, printer, 0);
    result.open(printer.outputFileName());
    if (hasInk(result.render(0)) || snapshot.fields(0).isEmpty()) return 14;
    return 0;
}
