#include "jpdf_desk/printing/pdf_printing.h"
#include "jpdf_desk/document/pdf_document.h"

#include <QPageRanges>
#include <QPainter>
#include <QPrinter>
#include <algorithm>
#include <cmath>
#include <stdexcept>

QVector<int> selectedPrintPages(const QPrinter &printer, int pageCount, int currentPage,
                                const PrintOptions &options)
{
    QVector<int> pages;
    for (int page = 0; page < pageCount; ++page) {
        bool selected = true;
        if (printer.printRange() == QPrinter::CurrentPage)
            selected = page == currentPage;
        else if (printer.printRange() == QPrinter::PageRange) {
            const QPageRanges ranges = printer.pageRanges();
            selected = !ranges.isEmpty() ? ranges.contains(page + 1)
                : page + 1 >= printer.fromPage() && page + 1 <= printer.toPage();
        } else if (printer.printRange() == QPrinter::Selection) {
            throw std::runtime_error("Printing an item selection is not supported. Choose a page range instead.");
        }
        if (options.subset == PrintOptions::PageSubset::Odd && (page + 1) % 2 == 0) selected = false;
        if (options.subset == PrintOptions::PageSubset::Even && (page + 1) % 2 != 0) selected = false;
        if (selected) pages.append(page);
    }
    if (printer.pageOrder() == QPrinter::LastPageFirst)
        std::reverse(pages.begin(), pages.end());
    return pages;
}

void printDocumentSnapshot(const PdfDocument &document, const DocumentAnnotations &annotations,
                           QPrinter &printer, int currentPage, const PrintOptions &options)
{
    const auto snapshot = document.snapshot(annotations);
    printDocument(*snapshot, printer, currentPage, options);
}

void printDocument(const PdfDocument &document, QPrinter &printer, int currentPage,
                   const PrintOptions &options)
{
    if (!QVector<int>({1, 2, 4, 6, 9, 16}).contains(options.pagesPerSheet)
        || !std::isfinite(options.scalePercent) || options.scalePercent < 10 || options.scalePercent > 200)
        throw std::runtime_error("Invalid print layout settings.");
    const auto selected = selectedPrintPages(printer, document.pageCount(), currentPage, options);
    if (selected.isEmpty()) throw std::runtime_error("No document pages selected for printing.");
    QVector<QVector<int>> sheets;
    for (int i = 0; i < selected.size(); i += options.pagesPerSheet)
        sheets.append(selected.mid(i, options.pagesPerSheet));
    QVector<QVector<int>> output;
    const int copies = printer.supportsMultipleCopies() ? 1 : qMax(1, printer.copyCount());
    if (printer.collateCopies()) {
        for (int copy = 0; copy < copies; ++copy) output += sheets;
    } else {
        for (const auto &sheet : sheets)
            for (int copy = 0; copy < copies; ++copy) output.append(sheet);
    }
    printer.setFullPage(false);
    QPainter painter;
    if (!painter.begin(&printer)) throw std::runtime_error("Cannot start the print job.");
    try {
        // Keep raster memory bounded independently of the printer's native DPI.
        const int dpi = qBound(72, printer.resolution(), 300);
        const int columns = options.pagesPerSheet == 1 ? 1
            : options.pagesPerSheet <= 4 ? 2 : options.pagesPerSheet <= 9 ? 3 : 4;
        const int rows = (options.pagesPerSheet + columns - 1) / columns;
        const QRectF printable = printer.pageRect(QPrinter::DevicePixel);
        const QSizeF cell(printable.width() / columns, printable.height() / rows);
        for (int i = 0; i < output.size(); ++i) {
            if (i > 0 && !printer.newPage()) throw std::runtime_error("Cannot print the next page.");
            for (int slot = 0; slot < output[i].size(); ++slot) {
                QImage image = document.renderForPrint(output[i][slot], dpi);
                // Enforce grayscale even if the driver does not convert raster images.
                if (printer.colorMode() == QPrinter::GrayScale)
                    image = image.convertToFormat(QImage::Format_Grayscale8);
                int column = slot % columns;
                if (options.rightToLeft) column = columns - 1 - column;
                const QRectF area(QPointF(column * cell.width(), (slot / columns) * cell.height()), cell);
                QSizeF size = image.size();
                if (options.scaling == PrintOptions::Scaling::Fit)
                    size.scale(cell, Qt::KeepAspectRatio);
                else
                    size *= double(printer.resolution()) / dpi;
                size *= options.scalePercent / 100;
                const QRectF target(area.center() - QPointF(size.width() / 2, size.height() / 2), size);
                painter.save();
                painter.setClipRect(area);
                painter.drawImage(target, image);
                painter.restore();
            }
            if (printer.printerState() == QPrinter::Error || printer.printerState() == QPrinter::Aborted)
                throw std::runtime_error("The print job failed or was aborted.");
        }
        if (!painter.end() || printer.printerState() == QPrinter::Error ||
            printer.printerState() == QPrinter::Aborted)
            throw std::runtime_error("Cannot finish the print job.");
    } catch (...) {
        printer.abort();
        if (painter.isActive()) painter.end();
        throw;
    }
}
