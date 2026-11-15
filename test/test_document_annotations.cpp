#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <cstring>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Exercise Unicode directories and filenames with Windows-legal punctuation.
    // Escapes keep the test independent of the compiler's source code page.
    QTemporaryDir directory(QDir::tempPath() + QStringLiteral("/jpdf-\u017e\u010d\u00e9-\u6587\u4ef6-XXXXXX"));
    if (!directory.isValid()) return 1;
    const QString input = directory.filePath(QStringLiteral("\u017dlu\u0165ou\u010dk\u00fd, [input] (1) & 'test' #100%.pdf"));
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    pdf_obj *pageObject = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, doc, -1, pageObject);
    pdf_drop_obj(ctx, pageObject);
    pdf_save_document(ctx, doc, input.toUtf8().constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);

    QImage image(20, 10, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return 2;
    const DocumentAnnotations edits{
        {{0, {{{30, 40, 200, 50}, "Text", 12}}}},
        {{0, {{OptionMark::Check, {50, 100}}, {OptionMark::Cross, {100, 100}}}}},
        {{0, {{{100, 200, 40, 20}, png}}}}};
    const QString current = directory.filePath(QStringLiteral("\u00e9, [current] (2) & #100%.pdf"));
    PdfDocument document;
    document.open(input);
    document.saveSnapshot(current, edits, png);

    // Verify current ownership and preserve annotations from other applications.
    doc = pdf_open_document(ctx, current.toUtf8().constData());
    pdf_page *page = pdf_load_page(ctx, doc, 0);
    for (pdf_annot *annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot)) {
        if (std::strcmp(pdf_annot_author(ctx, annot), "jPDF Desk") != 0) return 3;
    }
    // Other applications' annotations must not be removed by ownership matching.
    pdf_annot *unrelated = pdf_create_annot(ctx, page, PDF_ANNOT_FREE_TEXT);
    pdf_set_annot_author(ctx, unrelated, "Another app");
    pdf_set_annot_rect(ctx, unrelated, {10, 10, 100, 30});
    pdf_set_annot_contents(ctx, unrelated, "Leave me intact");
    pdf_drop_page(ctx, page);
    const QString annotated = directory.filePath("annotated.pdf");
    pdf_save_document(ctx, doc, annotated.toUtf8().constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);

    document.open(annotated);
    if (document.fields(0).size() != 1 || document.marks(0).size() != 2 ||
        document.signatures(0).size() != 1 || document.signatureTemplate() != png) return 4;
    if (std::abs(document.fields(0)[0].rect.y() - edits.fields[0][0].rect.y()) > .01) return 5;
    const QString output = directory.filePath(QStringLiteral("\u6587\u4ef6, [output] (3) & 'saved' #100%.pdf"));
    auto retainedEdits = edits;
    retainedEdits.signatures[0] = document.signatures(0);
    document.saveSnapshot(output, retainedEdits, png);
    doc = pdf_open_document(ctx, output.toUtf8().constData());
    page = pdf_load_page(ctx, doc, 0);
    int owned = 0, others = 0;
    for (pdf_annot *annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot)) {
        const QByteArray author(pdf_annot_author(ctx, annot));
        if (author == "jPDF Desk") ++owned;
        else if (author == "Another app") ++others;
        else return 6;
    }
    pdf_drop_page(ctx, page);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
    if (owned != 3 || others != 1) return 7;
    // Failed annotation processing must preserve the existing destination and
    // all live annotations (even edits processed before the invalid page).
    document.open(annotated);
    document.setMetadata({{"Title", "Pending metadata"}});
    document.setPassword("pending-secret");
    const QString failedOutput = directory.filePath("failed.pdf");
    QFile sentinel(failedOutput);
    if (!sentinel.open(QIODevice::WriteOnly) || sentinel.write("unchanged") != 9) return 9;
    sentinel.close();
    auto failedEdits = DocumentAnnotations{{{0, {}}, {99, {}}}, {{0, {}}}, {{0, {}}}};
    try {
        document.saveSnapshot(failedOutput, failedEdits);
        return 10;
    } catch (const std::exception &) {}
    if (!sentinel.open(QIODevice::ReadOnly) || sentinel.readAll() != "unchanged") return 11;
    sentinel.close();
    if (document.path() != annotated || document.fields(0).size() != 1
        || document.marks(0).size() != 2 || document.signatures(0).size() != 1
        || document.signatureTemplate() != png
        || document.metadata().value("Title") != "Pending metadata") return 12;
    // Failure to publish after valid edits must likewise leave live state intact.
    try {
        document.saveSnapshot(directory.filePath("missing/output.pdf"), {{{0, {}}}, {{0, {}}}, {{0, {}}}});
        return 13;
    } catch (const std::exception &) {}
    if (document.fields(0).size() != 1 || document.marks(0).size() != 2
        || document.signatures(0).size() != 1) return 14;
    document.saveSnapshot(directory.filePath("retry.pdf"), {});
    PdfDocument retry;
    retry.open(directory.filePath("retry.pdf"), "pending-secret");
    if (retry.fields(0).size() != 1 || retry.metadata().value("Title") != "Pending metadata") return 15;
    // In-memory snapshots preserve metadata, but ordinary exports strip the
    // reusable PNG even when no page edits were requested. Placed images stay.
    PdfDocument privacy;
    privacy.open(annotated);
    if (privacy.snapshot({})->signatureTemplate() != png) return 16;
    privacy.saveSnapshot(directory.filePath("private-export.pdf"), {});
    if (!privacy.signatureTemplate().isEmpty() || privacy.signatures(0).size() != 1 ||
        privacy.fields(0).size() != 1 || privacy.marks(0).size() != 2) return 17;
    // Explicit template bytes remain an opt-in for callers needing portability.
    privacy.saveSnapshot(directory.filePath("opt-in-template.pdf"), {}, png);
    if (privacy.signatureTemplate() != png) return 18;
    privacy.saveSnapshot(directory.filePath("cleared-template.pdf"), {}, QByteArray());
    if (!privacy.signatureTemplate().isEmpty()) return 19;
    // Deletions must remove annotations owned by the application.
    document.open(annotated);
    document.saveSnapshot(directory.filePath("deleted.pdf"), {{{0, {}}}, {{0, {}}}, {{0, {}}}});
    if (!document.fields(0).isEmpty() || !document.marks(0).isEmpty() ||
        !document.signatures(0).isEmpty()) return 8;
    return 0;
}
