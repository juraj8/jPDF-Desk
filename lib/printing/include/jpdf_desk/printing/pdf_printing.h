#pragma once

#include "jpdf_desk/document/pdf_types.h"

class PdfDocument;
class QPrinter;

struct PrintOptions {
    enum class PageSubset { All, Odd, Even };
    enum class Scaling { Fit, ActualSize };
    int pagesPerSheet = 1;
    bool rightToLeft = false;
    PageSubset subset = PageSubset::All;
    Scaling scaling = Scaling::Fit;
    double scalePercent = 100;
};

// Page numbers are zero-based; the printer dialog uses one-based ranges.
QVector<int> selectedPrintPages(const QPrinter &printer, int pageCount, int currentPage,
                                const PrintOptions &options = {});
// Render selected pages with the requested sheet layout; defaults fit each page.
void printDocument(const PdfDocument &document, QPrinter &printer, int currentPage,
                   const PrintOptions &options = {});
// Include unsaved edits without modifying the source PDF, its path, or signatures.
void printDocumentSnapshot(const PdfDocument &document, const DocumentAnnotations &annotations,
                           QPrinter &printer, int currentPage, const PrintOptions &options = {});
