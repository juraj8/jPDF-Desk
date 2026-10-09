#include "document_loading.h"

LoadedPdf loadPdf(const PdfSource &source, qreal resolution, const PdfTaskProgress &progress)
{
    LoadedPdf result;
    result.document = std::make_unique<PdfDocument>();
    progress.checkCancelled();
    result.document->open(source.path, source.password);
    const auto &doc = *result.document;
    const int count = doc.pageCount();
    result.pages.reserve(count);
    for (int page = 0; page < count; ++page) {
        progress.report(page, count);
        result.pages.append({doc.pageSize(page), {doc.fields(page), doc.marks(page), doc.signatures(page)}});
    }
    progress.checkCancelled();
    try {
        result.outline = doc.outline();
    } catch (const std::exception &e) {
        // A malformed outline must not prevent opening otherwise readable pages.
        result.outlineError = QString::fromUtf8(e.what());
    }
    result.signatureTemplate = doc.signatureTemplate();
    result.resolution = resolution;
    if (count) result.firstPage = doc.render(0, resolution);
    // Prime the initially visible thumbnail rows without rasterizing every page.
    for (int page = 0; page < qMin(count, 4); ++page) {
        progress.checkCancelled();
        const auto size = result.pages[page].size;
        const int dpi = qMax(1, int(qMin(160.0 / size.width(), 200.0 / size.height()) * 72.0 * 1.5));
        result.thumbnails.append(doc.renderForPrint(page, dpi));
    }
    progress.report(count, count);
    return result;
}
