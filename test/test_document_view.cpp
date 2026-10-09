#include "document_view.h"
#include "ui/editable_text.h"
#include "ui/page_annotations.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPushButton>
#include <QTemporaryDir>

namespace {
void makePdf(const QString &path)
{
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    for (int i = 0; i < 2; ++i) {
        pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 600, 800}, 0, nullptr, nullptr);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    pdf_save_document(ctx, doc, QFile::encodeName(path).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}

QGraphicsPixmapItem *pageImage(DocumentView &view, int page)
{
    for (auto *item : view.scene()->items()) {
        auto *image = qgraphicsitem_cast<QGraphicsPixmapItem *>(item);
        if (image && image->parentItem() && image->parentItem()->data(0).toInt() == page)
            return image;
    }
    return nullptr;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(jpdf_desk_ui);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString path = dir.filePath("input.pdf");
    makePdf(path);
    PdfDocument pdf;
    DocumentView view(pdf);
    view.resize(700, 500);
    int current = -1, count = -1, searchIndex = 0, searchCount = 0;
    bool error = false;
    QObject::connect(&view, &DocumentView::currentPageChanged, &view, [&](int page, int pages) {
        current = page;
        count = pages;
    });
    QObject::connect(&view, &DocumentView::searchResultChanged, &view, [&](int index, int matches) {
        searchIndex = index;
        searchCount = matches;
    });
    QObject::connect(&view, &DocumentView::pdfError, &view, [&](const QString &) { error = true; });
    view.setDarkTheme(true);
    view.showDocument();
    view.addText();
    view.addMark(OptionMark::Check);
    view.goToPage(1);
    if (current != 0 || count != 0 || !view.captureDrafts().fields.isEmpty()) return 2;
    pdf.open(path);
    view.showDocument(true);
    view.show();
    app.processEvents();
    if (current != 0 || count != 2 || view.currentPage() != 0) return 3;
    auto *firstImage = pageImage(view, 0);
    auto *secondImage = pageImage(view, 1);
    if (!firstImage || !secondImage || firstImage->pixmap().isNull()
        || !secondImage->pixmap().isNull()) return 4;
    view.addText();
    auto *text = qgraphicsitem_cast<EditableText *>(view.scene()->selectedItems().first());
    if (!text) return 5;
    text->setPlainText(QStringLiteral("Unsaved draft"));
    view.addMark(OptionMark::Cross);
    view.setDarkTheme(false);
    auto draft = view.captureDrafts();
    if (draft.fields.size() != 2 || draft.fields[0].size() != 1
        || draft.fields[0].first().text != QStringLiteral("Unsaved draft")
        || draft.marks[0].size() != 1 || !draft.fields[1].isEmpty()) return 6;
    view.navigatePage(1);
    app.processEvents();
    if (current != 1 || view.currentPage() != 1 || !firstImage->pixmap().isNull()
        || secondImage->pixmap().isNull()) return 7;
    view.goToPage(-1);
    view.goToPage(2);
    if (view.currentPage() != 1) return 8;
    QImage image(100, 20, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    view.placeSignature(png);
    draft = view.captureDrafts();
    if (draft.signatures[1].size() != 1 || draft.signatures[1].first().png != png
        || draft.signatures[1].first().rect.y() < 0
        || draft.signatures[1].first().rect.y() >= pdf.pageSize(1).height()) return 9;
    for (auto *item : view.scene()->items())
        if (isAnnotation(item)) delete item;
    draft = view.captureDrafts();
    // Every page remains present even after its final annotation is removed.
    if (draft.fields.size() != 2 || draft.marks.size() != 2 || draft.signatures.size() != 2
        || !draft.fields[0].isEmpty() || !draft.marks[0].isEmpty()
        || !draft.signatures[1].isEmpty()) return 10;
    view.findText(QStringLiteral("absent"));
    if (searchIndex != -1 || searchCount != 0) return 11;
    view.showDocument();
    if (view.currentPage() != 1 || searchIndex != -1 || searchCount != -1) return 12;
    view.showDocument(true);
    view.resize(700, 1600);
    app.processEvents();
    // Resizing brings another page into view and must populate its raster.
    if (pageImage(view, 1)->pixmap().isNull()) return 13;
    view.resize(700, 500);
    app.processEvents();
    view.goToPage(0);
    if (!pageImage(view, 1)->pixmap().isNull() || error) return 14;
    // Destruction with a live selection must not access deleted overlays.
    view.addText();
    return 0;
}
