#include "main_window.h"
#include "jpdf_desk/document/pdf_document.h"
#include "ui/editable_text.h"
#include "ui/page_annotations.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>

namespace {
void makePdf(const QString &path)
{
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    pdf_obj *resources = pdf_new_dict(ctx, doc, 1);
    pdf_obj *fonts = pdf_dict_put_dict(ctx, resources, PDF_NAME(Font), 1);
    pdf_obj *font = pdf_new_dict(ctx, doc, 3);
    pdf_dict_put(ctx, font, PDF_NAME(Type), PDF_NAME(Font));
    pdf_dict_put(ctx, font, PDF_NAME(Subtype), PDF_NAME(Type1));
    pdf_dict_put_name(ctx, font, PDF_NAME(BaseFont), "Helvetica");
    pdf_dict_puts_drop(ctx, fonts, "F1", font);
    for (const QByteArray &contents : {
             QByteArray("BT /F1 12 Tf 30 300 Td (Hello world Hello) Tj 0 -20 Td (second line) Tj ET"),
             QByteArray("BT /F1 12 Tf 30 300 Td (HELLO again) Tj ET")}) {
        fz_buffer *buffer = fz_new_buffer_from_copied_data(ctx,
            reinterpret_cast<const unsigned char *>(contents.constData()), contents.size());
        pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, resources, buffer);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
        fz_drop_buffer(ctx, buffer);
    }
    pdf_drop_obj(ctx, resources);
    pdf_save_document(ctx, doc, QFile::encodeName(path).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}

QVector<QGraphicsRectItem *> highlights(QGraphicsScene *scene)
{
    QVector<QGraphicsRectItem *> result;
    for (auto *item : scene->items()) {
        auto *rect = qgraphicsitem_cast<QGraphicsRectItem *>(item);
        if (rect && rect->zValue() == 10 && rect->parentItem()) result.append(rect);
    }
    return result;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString path = dir.filePath("search.pdf");
    makePdf(path);
    PdfDocument pdf;
    if (!pdf.search("hello").isEmpty()) return 2;
    pdf.open(path);
    const auto matches = pdf.search("hElLo");
    if (matches.size() != 3 || matches[0].page != 0 || matches[1].page != 0 || matches[2].page != 1)
        return 3;
    // Bounds use the same 1.5x coordinates as the document scene.
    if (matches[0].rects.isEmpty() || qAbs(matches[0].rects[0].left() - 45) > 1
        || matches[0].rects[0].width() <= 0) return 4;
    if (pdf.search("world Hello").size() != 1 || !pdf.search("absent").isEmpty()
        || !pdf.search("  ").isEmpty()) return 5;
    const auto multiline = pdf.search("Hello second");
    if (multiline.size() != 1 || multiline[0].rects.size() < 2) return 6;

    MainWindow window;
    window.show();
    auto *query = window.findChild<QLineEdit *>("searchQuery");
    auto *next = window.findChild<QPushButton *>("nextMatch");
    auto *previous = window.findChild<QPushButton *>("previousMatch");
    auto *count = window.findChild<QLabel *>("searchResultCount");
    if (!query || !next || !previous || !count || query->isEnabled() || next->isEnabled()) return 7;
    window.openDocument(path);
    app.processEvents();
    if (!query->isEnabled()) return 8;
    auto *scene = window.findChild<QGraphicsScene *>();
    auto *draft = new EditableText({{20, 20, 150, 40}, "Unsaved edit", 12});
    QGraphicsItem *root = nullptr;
    for (auto *item : scene->items())
        if (!item->parentItem() && item->data(0).isValid() && item->data(0).toInt() == 0) root = item;
    if (!root) return 9;
    draft->setParentItem(root);
    query->setText("hello");
    query->returnPressed();
    if (count->text() != "1 / 3" || !next->isEnabled() || highlights(scene).isEmpty()) return 10;
    previous->click(); // Wrap to the last result on the second page.
    if (count->text() != "3 / 3" || highlights(scene).first()->parentItem()->data(0).toInt() != 1) return 11;
    next->click();
    if (count->text() != "1 / 3") return 12;
    query->returnPressed();
    if (count->text() != "2 / 3") return 13;
    if (captureAnnotations(*scene, root).fields.size() != 1
        || captureAnnotations(*scene, root).fields[0].text != "Unsaved edit") return 14;
    query->setText("absent");
    if (!highlights(scene).isEmpty() || next->isEnabled() || !count->text().isEmpty()) return 15;
    query->returnPressed();
    if (count->text() != "No matches") return 16;
    query->clear();
    if (!count->text().isEmpty() || previous->isEnabled()) return 17;
    query->setText("hello");
    query->returnPressed();
    window.openDocument(path); // Successful document replacement discards old results.
    if (!highlights(scene).isEmpty() || next->isEnabled() || !count->text().isEmpty()) return 18;
    return 0;
}
