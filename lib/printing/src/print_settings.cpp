#include "jpdf_desk/printing/print_settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPageRanges>
#include <cmath>

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("PrintDialog", text);
}
}

QString PrintSettings::validate(int pageCount, int currentPage, Purpose purpose) const
{
    if (range != QPrinter::AllPages && range != QPrinter::CurrentPage && range != QPrinter::PageRange)
        return tr("Printing an item selection is not supported. Choose a page range instead.");
    const QPageRanges selected = QPageRanges::fromString(pageRange);
    if (range == QPrinter::PageRange && (selected.isEmpty() || selected.lastPage() > pageCount))
        return tr("Enter valid page numbers between 1 and %1 (for example: 1-3, 5).").arg(pageCount);
    if (!QVector<int>({1, 2, 4, 6, 9, 16}).contains(options.pagesPerSheet)
        || !std::isfinite(options.scalePercent) || options.scalePercent < 10 || options.scalePercent > 200
        || copies < 1 || copies > 999 || resolution < 0)
        return tr("Invalid print layout settings.");
    QPrinter selectionProbe;
    selectionProbe.setPageRanges(range == QPrinter::PageRange ? selected : QPageRanges());
    selectionProbe.setPrintRange(range);
    if (selectedPrintPages(selectionProbe, pageCount, currentPage, options).isEmpty())
        return tr("No pages match the selected range and odd/even filter.");
    const QFileInfo target(outputPath.trimmed());
    if (purpose == Purpose::Print && printerName.isEmpty()
        && (outputPath.trimmed().isEmpty() || !target.isAbsolute()
            || target.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0
            || target.exists() || !target.dir().exists()))
        return tr("Enter a full path to a new .pdf file in an existing folder. Existing files will not be overwritten.");
    return {};
}

QString PrintSettings::apply(QPrinter &printer, int pageCount, int currentPage, Purpose purpose) const
{
    const QString error = validate(pageCount, currentPage, purpose);
    if (!error.isEmpty()) return error;
    const bool preview = purpose == Purpose::Preview;
    if (!preview) {
        if (printerName.isEmpty()) {
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(QFileInfo(outputPath.trimmed()).absoluteFilePath());
        } else {
            printer.setOutputFileName(QString());
            printer.setPrinterName(printerName);
            printer.setOutputFormat(QPrinter::NativeFormat);
        }
    }
    // Resolution must be configured before starting the painter.
    if (resolution > 0) printer.setResolution(resolution);
    printer.setDocName(jobName);
    printer.setPaperSource(paperSource);
    printer.setColorMode(colorMode);
    printer.setPageSize(pageSize);
    printer.setPageOrientation(orientation);
    printer.setDuplex(duplex);
    printer.setCopyCount(preview ? 1 : copies);
    printer.setCollateCopies(collate);
    printer.setPageOrder(reverse ? QPrinter::LastPageFirst : QPrinter::FirstPageFirst);
    printer.setPageRanges(range == QPrinter::PageRange ? QPageRanges::fromString(pageRange) : QPageRanges());
    printer.setPrintRange(range);
    return {};
}
