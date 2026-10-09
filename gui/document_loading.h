#pragma once

#include "jpdf_desk/document/pdf_document.h"
#include "pdf_task.h"

struct PreparedPage {
    QSizeF size;
    PageAnnotations annotations;
};

struct LoadedPdf {
    std::unique_ptr<PdfDocument> document;
    QVector<PreparedPage> pages;
    QVector<OutlineEntry> outline;
    QString outlineError;
    QByteArray signatureTemplate;
    QImage firstPage;
    QVector<QImage> thumbnails;
    qreal resolution = 1;
};

// Worker-only loading, authentication, page discovery, and initial rasterization.
// The returned document can be handed to the GUI only after the worker is joined.
LoadedPdf loadPdf(const PdfSource &source, qreal resolution, const PdfTaskProgress &progress);
