#include "jpdf_desk/printing/print_settings.h"

#include <QGuiApplication>
#include <QFile>
#include <QTemporaryDir>
#include <limits>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    using Purpose = PrintSettings::Purpose;
    PrintSettings settings;
    QPrinter printer;
    printer.setDocName(QStringLiteral("Untouched"));
    const auto failsWithoutMutation = [&] {
        return !settings.apply(printer, 8, 3, Purpose::Print).isEmpty()
            && printer.docName() == QStringLiteral("Untouched")
            && printer.outputFileName().isEmpty() && printer.printRange() == QPrinter::AllPages;
    };
    if (!failsWithoutMutation()) return 2;
    if (!settings.validate(8, 3, Purpose::Preview).isEmpty()) return 3;
    for (const auto &path : {QStringLiteral("relative.pdf"), directory.filePath(QStringLiteral("output.txt")),
                             directory.filePath(QStringLiteral("missing/output.pdf"))}) {
        settings.outputPath = path;
        if (!failsWithoutMutation()) return 4;
    }
    settings.outputPath = directory.filePath(QStringLiteral("output.pdf"));
    QFile existing(settings.outputPath);
    if (!existing.open(QIODevice::WriteOnly)) return 5;
    existing.close();
    if (!failsWithoutMutation()) return 6;
    existing.remove();
    settings.range = QPrinter::PageRange;
    for (const auto &range : {QString(), QStringLiteral("9"), QStringLiteral("abc")}) {
        settings.pageRange = range;
        if (!failsWithoutMutation() || settings.validate(8, 3, Purpose::Preview).isEmpty()) return 7;
    }
    settings.pageRange = QStringLiteral("2, 4");
    settings.options.subset = PrintOptions::PageSubset::Odd;
    if (!failsWithoutMutation()) return 8;
    settings.options.subset = PrintOptions::PageSubset::All;
    settings.range = QPrinter::Selection;
    if (!failsWithoutMutation()) return 9;
    settings.range = QPrinter::PageRange;
    settings.options.scalePercent = std::numeric_limits<double>::quiet_NaN();
    if (!failsWithoutMutation()) return 10;
    settings.options.scalePercent = 80;
    settings.options.pagesPerSheet = 3;
    if (!failsWithoutMutation()) return 11;
    settings.options.pagesPerSheet = 2;
    settings.pageRange = QStringLiteral("1-3, 5, 8");
    settings.reverse = true;
    settings.colorMode = QPrinter::GrayScale;
    settings.resolution = 150;
    settings.jobName = QStringLiteral("Test job");
    settings.pageSize = QPageSize(QPageSize::Letter);
    settings.orientation = QPageLayout::Landscape;
    settings.copies = 3;
    settings.collate = false;
    if (!settings.apply(printer, 8, 3, Purpose::Print).isEmpty()) return 12;
    if (printer.outputFormat() != QPrinter::PdfFormat || printer.outputFileName() != settings.outputPath
        || printer.docName() != settings.jobName || printer.resolution() != 150
        || printer.colorMode() != QPrinter::GrayScale || printer.copyCount() != 3 || printer.collateCopies()
        || printer.pageLayout().orientation() != QPageLayout::Landscape
        || !printer.pageLayout().pageSize().isEquivalentTo(settings.pageSize)
        || selectedPrintPages(printer, 8, 3, settings.options) != QVector<int>({7, 4, 2, 1, 0})) return 13;
    // Preview preserves its own destination and suppresses copies.
    QPrinter preview;
    preview.setOutputFormat(QPrinter::PdfFormat);
    const QString previewPath = directory.filePath(QStringLiteral("preview.pdf"));
    preview.setOutputFileName(previewPath);
    settings.outputPath.clear();
    if (!settings.apply(preview, 8, 3, Purpose::Preview).isEmpty()
        || preview.outputFileName() != previewPath || preview.copyCount() != 1
        || preview.colorMode() != printer.colorMode() || preview.pageRanges() != printer.pageRanges()) return 14;
    settings.range = QPrinter::CurrentPage;
    settings.options.subset = PrintOptions::PageSubset::Odd;
    if (settings.validate(8, 3, Purpose::Preview).isEmpty()) return 15;
    settings.options.subset = PrintOptions::PageSubset::Even;
    if (!settings.apply(preview, 8, 3, Purpose::Preview).isEmpty()
        || selectedPrintPages(preview, 8, 3, settings.options) != QVector<int>({3})) return 16;
    settings.range = QPrinter::AllPages;
    if (settings.validate(0, 0, Purpose::Preview).isEmpty()) return 17;
    return 0;
}
