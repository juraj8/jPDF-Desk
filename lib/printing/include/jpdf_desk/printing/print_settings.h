#pragma once

#include "jpdf_desk/printing/pdf_printing.h"
#include <QPageSize>
#include <QPrinter>

// A snapshot of a print job, independent of the settings controls.
struct PrintSettings {
    enum class Purpose { Print, Preview };

    QString printerName; // Empty selects PDF output.
    QString outputPath;
    QPrinter::PrintRange range = QPrinter::AllPages;
    QString pageRange;
    PrintOptions options;
    int resolution = 0; // Zero preserves the printer default.
    QString jobName;
    QPrinter::PaperSource paperSource = QPrinter::Auto;
    QPrinter::ColorMode colorMode = QPrinter::Color;
    QPageSize pageSize = QPageSize(QPageSize::A4);
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QPrinter::DuplexMode duplex = QPrinter::DuplexNone;
    int copies = 1;
    bool collate = true;
    bool reverse = false;

    // Empty means valid. Preview does not require an output filename.
    QString validate(int pageCount, int currentPage, Purpose purpose) const;
    // Validates before mutating the printer; returns an error or an empty string.
    QString apply(QPrinter &printer, int pageCount, int currentPage, Purpose purpose) const;
};
