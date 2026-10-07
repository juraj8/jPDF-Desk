#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <cstring>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString input = directory.filePath("input.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    pdf_obj *pageObject = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, doc, -1, pageObject);
    pdf_drop_obj(ctx, pageObject);
    pdf_save_document(ctx, doc, QFile::encodeName(input).constData(), &pdf_default_write_options);
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
    const QString current = directory.filePath("current.pdf");
    PdfDocument document;
    document.open(input);
    document.saveSnapshot(current, edits, png);

    // Convert the fixture to the identifiers written by older releases.
    doc = pdf_open_document(ctx, QFile::encodeName(current).constData());
    pdf_page *page = pdf_load_page(ctx, doc, 0);
    for (pdf_annot *annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot)) {
        if (std::strcmp(pdf_annot_author(ctx, annot), "jPDF Desk") != 0) return 3;
        pdf_set_annot_author(ctx, annot, "PDF Filler");
        QByteArray subject(pdf_annot_subject(ctx, annot));
        subject.replace("jPDF Desk", "PDF Filler");
        pdf_set_annot_subject(ctx, annot, subject.constData());
    }
    // Other applications' annotations must not be removed by legacy matching.
    pdf_annot *unrelated = pdf_create_annot(ctx, page, PDF_ANNOT_FREE_TEXT);
    pdf_set_annot_author(ctx, unrelated, "Another app");
    pdf_set_annot_rect(ctx, unrelated, {10, 10, 100, 30});
    pdf_set_annot_contents(ctx, unrelated, "Leave me intact");
    pdf_drop_page(ctx, page);
    fz_set_metadata(ctx, reinterpret_cast<fz_document *>(doc), "info:JPDFDeskSignature", "");
    fz_set_metadata(ctx, reinterpret_cast<fz_document *>(doc), "info:PdfFillerSignature",
                    png.toBase64().constData());
    const QString legacy = directory.filePath("legacy.pdf");
    pdf_save_document(ctx, doc, QFile::encodeName(legacy).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);

    document.open(legacy);
    if (document.fields(0).size() != 1 || document.marks(0).size() != 2 ||
        document.signatures(0).size() != 1 || document.signatureTemplate() != png) return 4;
    if (std::abs(document.fields(0)[0].rect.y() - edits.fields[0][0].rect.y()) > .01) return 5;
    const QString output = directory.filePath("output.pdf");
    document.saveSnapshot(output, edits, png);
    doc = pdf_open_document(ctx, QFile::encodeName(output).constData());
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
    if (owned != 4 || others != 1) return 7;
    // Deletions must also remove annotations saved under the old name.
    document.open(legacy);
    document.saveSnapshot(directory.filePath("deleted.pdf"), {{{0, {}}}, {{0, {}}}, {{0, {}}}});
    if (!document.fields(0).isEmpty() || !document.marks(0).isEmpty() ||
        !document.signatures(0).isEmpty()) return 8;
    return 0;
}
