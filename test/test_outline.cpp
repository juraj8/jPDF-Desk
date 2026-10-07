#include "main_window.h"
#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString input = dir.filePath("outline.pdf");
    const QString empty = dir.filePath("empty.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    for (int i = 0; i < 3; ++i) {
        pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 600, 800}, 0, nullptr, nullptr);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    pdf_save_document(ctx, doc, QFile::encodeName(empty).constData(), &pdf_default_write_options);
    pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    pdf_obj *outline = pdf_add_new_dict(ctx, doc, 4);
    pdf_obj *chapter = pdf_add_new_dict(ctx, doc, 6);
    pdf_obj *child = pdf_add_new_dict(ctx, doc, 4);
    pdf_obj *external = pdf_add_new_dict(ctx, doc, 4);
    pdf_dict_put(ctx, root, PDF_NAME(Outlines), outline);
    pdf_dict_put(ctx, outline, PDF_NAME(First), chapter);
    pdf_dict_put(ctx, outline, PDF_NAME(Last), external);
    pdf_dict_put_int(ctx, outline, PDF_NAME(Count), 3);
    pdf_dict_put_text_string(ctx, chapter, PDF_NAME(Title), "Chapter 1");
    pdf_dict_put(ctx, chapter, PDF_NAME(Parent), outline);
    pdf_dict_put(ctx, chapter, PDF_NAME(First), child);
    pdf_dict_put(ctx, chapter, PDF_NAME(Last), child);
    pdf_dict_put(ctx, chapter, PDF_NAME(Next), external);
    pdf_dict_put_int(ctx, chapter, PDF_NAME(Count), 1);
    pdf_obj *dest = pdf_dict_put_array(ctx, chapter, PDF_NAME(Dest), 2);
    pdf_array_push(ctx, dest, pdf_lookup_page_obj(ctx, doc, 0));
    pdf_array_push(ctx, dest, PDF_NAME(Fit));
    pdf_dict_put_text_string(ctx, child, PDF_NAME(Title), "Résumé — section 2");
    pdf_dict_put(ctx, child, PDF_NAME(Parent), chapter);
    dest = pdf_dict_put_array(ctx, child, PDF_NAME(Dest), 2);
    pdf_array_push(ctx, dest, pdf_lookup_page_obj(ctx, doc, 2));
    pdf_array_push(ctx, dest, PDF_NAME(Fit));
    pdf_dict_put_text_string(ctx, external, PDF_NAME(Title), "Website");
    pdf_dict_put(ctx, external, PDF_NAME(Parent), outline);
    pdf_dict_put(ctx, external, PDF_NAME(Prev), chapter);
    pdf_obj *action = pdf_dict_put_dict(ctx, external, PDF_NAME(A), 2);
    pdf_dict_put(ctx, action, PDF_NAME(S), PDF_NAME(URI));
    pdf_dict_put_text_string(ctx, action, PDF_NAME(URI), "https://example.com");
    pdf_save_document(ctx, doc, QFile::encodeName(input).constData(), &pdf_default_write_options);
    pdf_drop_obj(ctx, external);
    pdf_drop_obj(ctx, child);
    pdf_drop_obj(ctx, chapter);
    pdf_drop_obj(ctx, outline);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);

    PdfDocument pdf;
    if (!pdf.outline().isEmpty()) return 2;
    pdf.open(input);
    const auto entries = pdf.outline();
    if (entries.size() != 2 || entries[0].page != 0 || !entries[0].expanded ||
        entries[0].children.size() != 1 || entries[0].children[0].page != 2 ||
        entries[0].children[0].title != QString::fromUtf8("Résumé — section 2") ||
        entries[1].page != -1) return 3;
    pdf.save(dir.filePath("saved.pdf"), {});
    if (pdf.outline().size() != 2) return 4;

    MainWindow window;
    window.show();
    app.processEvents();
    auto *panel = window.findChild<QWidget *>("outlinePanel");
    auto *toggle = window.findChild<QPushButton *>("outlineToggle");
    auto *tree = window.findChild<QTreeWidget *>("outlineTree");
    auto *thumbnails = window.findChild<QListWidget *>("pageThumbnails");
    auto *outlineView = window.findChild<QPushButton *>("outlineViewButton");
    auto *thumbnailView = window.findChild<QPushButton *>("thumbnailViewButton");
    auto *pageLabel = window.findChild<QLabel *>("pageLabel");
    auto *header = window.findChild<QWidget *>("pageControls");
    auto *toolsToggle = window.findChild<QPushButton *>("sidebarToggle");
    if (!header || !toolsToggle || !toggle || !pageLabel || toggle->parentWidget() != header ||
        toolsToggle->parentWidget() != header || toggle->size() != toolsToggle->size()) return 14;
    auto *search = window.findChild<QWidget *>("searchControls");
    if (!search || search->parentWidget() != header) return 31;
    const auto centered = [&] {
        const int center = pageLabel->mapTo(header, pageLabel->rect().center()).x();
        auto *previous = window.findChild<QPushButton *>("previousButton");
        auto *next = window.findChild<QPushButton *>("nextButton");
        // Full-bar centering is preferred, constrained by the edge controls.
        const int minimumCenter = toolsToggle->geometry().right() + 9
            + center - previous->geometry().left();
        const int maximumCenter = search->geometry().left() - 9
            - (next->geometry().right() - center);
        if (minimumCenter > maximumCenter) return false;
        return qAbs(center - qBound(minimumCenter, header->rect().center().x(), maximumCenter)) <= 1;
    };
    if (!panel || !toggle || !tree || !thumbnails || thumbnails->count() ||
        !pageLabel || toggle->isEnabled() || !tree->isHidden()) return 5;
    window.openDocument(input);
    app.processEvents();
    if (!toggle->isEnabled() || !toggle->isChecked() || tree->isHidden() || tree->topLevelItemCount() != 2)
        return 6;
    if (!centered()) return 15;
    if (!outlineView || !thumbnailView || !outlineView->isChecked() ||
        thumbnailView->isChecked() || thumbnails->isVisible()) return 26;
    thumbnailView->click();
    app.processEvents();
    if (tree->isVisible() || !thumbnails->isVisible() || !thumbnailView->isChecked() ||
        outlineView->isChecked()) return 27;
    toggle->click();
    toggle->click();
    app.processEvents();
    if (!thumbnails->isVisible() || tree->isVisible() || !thumbnailView->isChecked()) return 28;
    if (thumbnails->count() != 3 || thumbnails->item(0)->icon().isNull() ||
        thumbnails->currentRow() != 0) return 19;
    thumbnails->itemClicked(thumbnails->item(1));
    app.processEvents();
    if (pageLabel->text() != "Page 2 / 3" || thumbnails->currentRow() != 1) return 20;
    thumbnails->scrollToItem(thumbnails->item(2));
    app.processEvents();
    if (thumbnails->item(2)->icon().isNull()) return 21;
    thumbnails->itemActivated(thumbnails->item(0));
    app.processEvents();
    if (pageLabel->text() != "Page 1 / 3") return 22;
    outlineView->click();
    app.processEvents();
    if (!tree->isVisible() || thumbnails->isVisible() || !outlineView->isChecked() ||
        thumbnailView->isChecked()) return 29;
    const auto fullHeightOutline = [&] {
        return panel->parentWidget() == window.centralWidget() && panel->y() == 0 &&
               panel->height() == window.centralWidget()->height();
    };
    if (!fullHeightOutline()) return 18;
    auto *item = tree->topLevelItem(0)->child(0);
    tree->itemClicked(item, 0);
    app.processEvents();
    if (pageLabel->text() != "Page 3 / 3" || thumbnails->currentRow() != 2) return 7;
    tree->itemClicked(tree->topLevelItem(1), 0); // External bookmarks do not navigate or launch a browser.
    if (pageLabel->text() != "Page 3 / 3") return 8;
    toggle->click();
    app.processEvents();
    if (!tree->isHidden() || !panel->isHidden() || toggle->isHidden() || !centered()) return 9;
    toggle->click();
    app.processEvents();
    if (tree->isHidden() || panel->isHidden() || !centered()) return 10;
    toolsToggle->click();
    app.processEvents();
    if (!centered()) return 16;
    toolsToggle->click();
    window.resize(1300, 850);
    app.processEvents();
    if (!centered() || !fullHeightOutline()) return 17;
    tree->itemActivated(tree->topLevelItem(0), 0);
    app.processEvents();
    if (pageLabel->text() != "Page 1 / 3") return 11;
    window.openDocument(empty);
    app.processEvents();
    if (tree->topLevelItemCount() || !toggle->isEnabled() || !tree->isHidden() ||
        panel->isHidden() || thumbnails->count() != 3 || thumbnails->item(0)->icon().isNull()) return 12;
    if (outlineView->isEnabled() || !thumbnailView->isChecked() || !thumbnails->isVisible()) return 30;
    thumbnails->itemClicked(thumbnails->item(2));
    app.processEvents();
    if (pageLabel->text() != "Page 3 / 3") return 23;
    toggle->click();
    if (!panel->isHidden()) return 24;
    toggle->click();
    app.processEvents();
    if (panel->isHidden()) return 25;
    pdf.open(empty);
    return pdf.outline().isEmpty() ? 0 : 13;
}
