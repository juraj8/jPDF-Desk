#include "document_view.h"
#include "ui/editable_text.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QFile>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QTemporaryDir>

namespace {
void makePdf(const QString &path)
{
    auto *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    auto *doc = pdf_create_document(ctx);
    for (int i = 0; i < 3; ++i) {
        auto *page = pdf_add_page(ctx, doc, {0, 0, 600, 800}, 0, nullptr, nullptr);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    auto *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    auto *dests = pdf_dict_put_dict(ctx, root, PDF_NAME(Dests), 1);
    auto *dest = pdf_dict_put_array(ctx, dests, pdf_new_name(ctx, "section"), 5);
    pdf_array_push(ctx, dest, pdf_lookup_page_obj(ctx, doc, 2));
    pdf_array_push(ctx, dest, PDF_NAME(XYZ));
    pdf_array_push_int(ctx, dest, 40);
    pdf_array_push_int(ctx, dest, 500);
    pdf_array_push(ctx, dest, PDF_NULL);
    auto *annots = pdf_dict_put_array(ctx, pdf_lookup_page_obj(ctx, doc, 0), PDF_NAME(Annots), 4);
    for (int i = 0; i < 4; ++i) {
        auto *link = pdf_array_push_dict(ctx, annots, 4);
        pdf_dict_put(ctx, link, PDF_NAME(Subtype), PDF_NAME(Link));
        pdf_dict_put_rect(ctx, link, PDF_NAME(Rect), {40.0f + i * 120, 700, 140.0f + i * 120, 740});
        if (i == 0) {
            pdf_dict_put(ctx, link, PDF_NAME(Dest), dest);
        } else {
            auto *action = pdf_dict_put_dict(ctx, link, PDF_NAME(A), 2);
            pdf_dict_put(ctx, action, PDF_NAME(S), i == 1 ? PDF_NAME(GoTo) : PDF_NAME(URI));
            if (i == 1)
                pdf_dict_put_name(ctx, action, PDF_NAME(D), "section");
            else
                pdf_dict_put_text_string(ctx, action, PDF_NAME(URI),
                                         i == 2 ? "https://example.com" : "file:///tmp/other.pdf");
        }
    }
    pdf_save_document(ctx, doc, QFile::encodeName(path).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}

void click(DocumentView &view, QPointF scenePoint, Qt::MouseButton button = Qt::LeftButton,
           QPoint delta = {})
{
    const QPoint pos = view.mapFromScene(scenePoint);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), QPointF(view.viewport()->mapToGlobal(pos)),
                      button, button, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos + delta),
                        QPointF(view.viewport()->mapToGlobal(pos + delta)),
                        button, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &release);
    QApplication::processEvents();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(jpdf_desk_ui);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const auto path = dir.filePath("links.pdf");
    makePdf(path);
    PdfDocument pdf;
    if (!pdf.links(0).isEmpty()) return 2;
    pdf.open(path);
    const auto links = pdf.links(0);
    if (links.size() != 2 || links[0].page != 2 || links[1].page != 2 ||
        links[0].rect != QRectF(60, 90, 150, 60) ||
        links[0].position != QPointF(60, 450) || links[1].position != links[0].position ||
        !pdf.links(1).isEmpty()) return 3;

    DocumentView view(pdf);
    view.resize(900, 500);
    view.showDocument(true);
    view.show();
    app.processEvents();
    const auto transform = view.transform();
    click(view, links[0].rect.center(), Qt::RightButton);
    if (view.currentPage() != 0) return 4;
    click(view, links[0].rect.center(), Qt::LeftButton, QPoint(30, 0));
    if (view.currentPage() != 0) return 5;
    click(view, links[0].rect.center());
    if (view.currentPage() != 2 || view.transform() != transform) return 6;
    // The XYZ destination is at the viewport top, not just the target page.
    if (qAbs(view.mapToScene(QPoint(0, 0)).y() - (2 * 1224 + 450)) > 3) return 7;
    view.goToPage(0);
    click(view, links[1].rect.center());
    if (view.currentPage() != 2) return 8;

    view.goToPage(0);
    auto *text = new EditableText({links[0].rect, QStringLiteral("Editable overlay"), 12});
    // Attach to the same page root as the link, above it in stacking order.
    for (auto *item : view.scene()->items()) {
        if (!item->parentItem() && item->data(0).isValid() && item->data(0).toInt() == 0) {
            text->setParentItem(item);
            break;
        }
    }
    click(view, links[0].rect.center());
    if (view.currentPage() != 0 || view.captureDrafts().fields[0].size() != 1) return 9;
    pdf.save(dir.filePath("saved.pdf"), {});
    if (pdf.links(0).size() != 2) return 10;
    view.showDocument(true);
    click(view, links[0].rect.center());
    if (view.currentPage() != 2) return 11;
    return 0;
}
