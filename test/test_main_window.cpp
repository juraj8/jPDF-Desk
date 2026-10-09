#include "main_window.h"
#ifdef JPDF_DESK_HAS_OPENSSL
#include "jpdf_desk/signing/openssl_provider.h"
#endif
#include "jpdf_desk/document/pdf_document.h"
#include "ui/signature_image.h"
#include "ui/editable_text.h"
#include "ui/mark_item.h"
#include "ui/signature_item.h"
#include "ui/page_annotations.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTimer>
#include <QBuffer>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include <QWheelEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <iostream>
#include <cmath>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    SignatureServices services;
#ifdef JPDF_DESK_HAS_OPENSSL
    services = openSslSignatureServices();
#endif
    MainWindow window(nullptr, services);
    if (window.windowTitle() != QStringLiteral("jPDF Desk")) return 1;
    auto *heading = window.findChild<QLabel *>(QStringLiteral("windowHeading"));
    if (!heading || heading->text() != QStringLiteral("jPDF Desk")) return 95;
    // Shared navigation setup must keep controls sized and accessible.
    for (const auto &name : {"sidebarToggle", "outlineToggle", "previousButton",
                             "nextButton", "zoomInButton", "zoomOutButton"}) {
        auto *button = window.findChild<QPushButton *>(QString::fromLatin1(name));
        if (!button || button->size() != QSize(36, 36) || button->toolTip().isEmpty()
            || button->accessibleName() != button->toolTip()) return 82;
    }
    // All shared theme placeholders must be resolved before applying the QSS.
    if (app.styleSheet().isEmpty() || app.styleSheet().contains(QLatin1Char('@'))) return 83;
    auto *logo = window.findChild<QLabel *>(QStringLiteral("brandLogo"));
    if (window.windowIcon().isNull() || !logo || !logo->pixmap() || logo->pixmap().isNull())
        return 21;
    auto *about = window.findChild<QPushButton *>(QStringLiteral("aboutButton"));
    if (!about || !about->isEnabled()) return 93;
    bool aboutDialogValid = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("aboutDialog"));
        if (!dialog) return;
        aboutDialogValid = dialog->windowTitle() == QStringLiteral("About jPDF Desk");
        for (const auto *name : {"aboutAuthor", "aboutDescription",
                                 "aboutHomepage", "aboutIssues", "aboutDonate", "aboutLicense", "aboutLicenseNotice"}) {
            auto *label = dialog->findChild<QLabel *>(QString::fromLatin1(name));
            if (!label || label->text().isEmpty()) aboutDialogValid = false;
        }
        for (const auto *name : {"aboutIssues", "aboutDonate"}) {
            auto *label = dialog->findChild<QLabel *>(QString::fromLatin1(name));
            if (!label || (label->text() != QStringLiteral("Not configured")
                && (!label->openExternalLinks() || !label->text().contains(QStringLiteral("<a href=\"")))))
                aboutDialogValid = false;
        }
        for (auto *label : dialog->findChildren<QLabel *>()) {
            if (label->wordWrap() && label->height() < label->heightForWidth(label->width()))
                aboutDialogValid = false;
        }
        auto *title = dialog->findChild<QLabel *>(QStringLiteral("windowHeading"));
        if (!title || title->textFormat() != Qt::RichText
            || !title->text().startsWith(QStringLiteral("jPDF Desk <small>"))
            || !title->text().endsWith(QStringLiteral("</small>"))
            || title->text().contains(QStringLiteral("<small></small>"))) aboutDialogValid = false;
        auto *author = dialog->findChild<QLabel *>(QStringLiteral("aboutAuthor"));
        if (!author || !author->text().contains(QLatin1Char('@'))
            || !author->text().endsWith(QLatin1Char('>'))) aboutDialogValid = false;
        for (const auto *name : {"aboutProject", "aboutVersion", "aboutPlatform", "aboutQtVersion"}) {
            if (dialog->findChild<QLabel *>(QString::fromLatin1(name))) aboutDialogValid = false;
        }
        dialog->reject();
    });
    about->click();
    if (!aboutDialogValid) return 94;
    int disabled = 0;
    for (auto *button : window.findChildren<QPushButton *>())
        if (!button->isEnabled()) ++disabled;
    if (disabled < 4) return 2;
    auto *verifySignatures = window.findChild<QPushButton *>(QStringLiteral("verifySignaturesButton"));
    if (!verifySignatures || verifySignatures->isEnabled()) return 70;
    auto *metadataButton = window.findChild<QPushButton *>(QStringLiteral("metadataButton"));
    if (!metadataButton || metadataButton->isEnabled()) return 85;
    auto *print = window.findChild<QPushButton *>(QStringLiteral("printButton"));
    if (!print || print->isEnabled()) return 72;
    auto *size = window.findChild<QSpinBox *>(QStringLiteral("textSize"));
    if (!size || size->minimum() > 8 || size->maximum() < 24) return 2;

    auto *zoomIn = window.findChild<QPushButton *>(QStringLiteral("zoomInButton"));
    auto *zoomOut = window.findChild<QPushButton *>(QStringLiteral("zoomOutButton"));
    auto *zoomBubble = window.findChild<QWidget *>(QStringLiteral("zoomControls"));
    if (!zoomIn || !zoomOut || !zoomBubble || zoomIn->isEnabled() || zoomOut->isEnabled())
        return 60;

    QTemporaryDir dir;
    if (!dir.isValid()) return 3;
    const QByteArray input = QFile::encodeName(dir.filePath("input.pdf"));
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *source = pdf_create_document(ctx);
    pdf_obj *page = pdf_add_page(ctx, source, {0, 0, 600, 800}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, source, -1, page);
    pdf_drop_obj(ctx, page);
    pdf_save_document(ctx, source, input.constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, source);
    fz_drop_context(ctx);

    PdfDocument pdf;
    pdf.open(QString::fromUtf8(input));
    if (pdf.pageCount() != 1 || pdf.render(0).isNull()) return 4;
    const QMap<QString, QString> properties = {
        {QStringLiteral("Title"), QStringLiteral("Résumé — テスト")},
        {QStringLiteral("Author"), QStringLiteral("Test author")},
        {QStringLiteral("Subject"), QStringLiteral("Test subject")},
        {QStringLiteral("Keywords"), QStringLiteral("one, two")},
        {QStringLiteral("Creator"), QStringLiteral("Test creator")},
        {QStringLiteral("Producer"), QStringLiteral("Test producer")}
    };
    pdf.setMetadata(properties);
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        if (pdf.metadata().value(it.key()) != it.value()) return 86;
    const QString output = dir.filePath("output.pdf");
    pdf.save(output, {{0, {{{50, 80, 250, 50}, QStringLiteral("Editable text"), 24}}}});
    PdfDocument metadataCopy;
    metadataCopy.open(output);
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        if (metadataCopy.metadata().value(it.key()) != it.value()) return 87;
    metadataCopy.setMetadata({{QStringLiteral("Author"), QString()}});
    metadataCopy.save(dir.filePath("cleared.pdf"), {});
    if (!metadataCopy.metadata().value(QStringLiteral("Author")).isEmpty()
        || metadataCopy.metadata().value(QStringLiteral("Title")) != properties.value(QStringLiteral("Title"))) return 88;
    // Exercise actual Graphics View hit testing: the handle must receive
    // a press, move and release rather than merely being painted.
    // The UI must reflect injected capabilities, not assume OpenSSL is present.
    MainWindow noCrypto;
    noCrypto.openDocument(output);
    if (noCrypto.findChild<QPushButton *>(QStringLiteral("signButton"))->isEnabled()
        || noCrypto.findChild<QPushButton *>(QStringLiteral("verifySignaturesButton"))->isEnabled()
        || !noCrypto.findChild<QPushButton *>(QStringLiteral("printButton"))->isEnabled()) return 84;
    window.openDocument(output);
    if (verifySignatures->isEnabled() != bool(services.verification)) return 71;
    if (!print->isEnabled()) return 73;
    if (!metadataButton->isEnabled()) return 89;
    bool metadataDialogValid = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("metadataDialog"));
        if (!dialog) return;
        auto *title = dialog->findChild<QLineEdit *>(QStringLiteral("metadataTitle"));
        auto *date = dialog->findChild<QLineEdit *>(QStringLiteral("metadataCreationDate"));
        metadataDialogValid = title && title->text() == properties.value(QStringLiteral("Title"))
            && date && date->isReadOnly();
        if (title) title->setText(QStringLiteral("Cancelled change"));
        dialog->reject();
    });
    metadataButton->click();
    if (!metadataDialogValid) return 90;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("metadataDialog"));
        if (!dialog) return;
        auto *title = dialog->findChild<QLineEdit *>(QStringLiteral("metadataTitle"));
        auto *subject = dialog->findChild<QPlainTextEdit *>(QStringLiteral("metadataSubject"));
        metadataDialogValid = title && title->text() == properties.value(QStringLiteral("Title"))
            && subject && subject->toPlainText() == properties.value(QStringLiteral("Subject"));
        if (title) title->setText(QStringLiteral("Updated title"));
        if (subject) subject->setPlainText(QStringLiteral("First line\nSecond line\nThird line"));
        dialog->accept();
    });
    metadataButton->click();
    if (!metadataDialogValid) return 91;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("metadataDialog"));
        if (!dialog) return;
        auto *title = dialog->findChild<QLineEdit *>(QStringLiteral("metadataTitle"));
        auto *subject = dialog->findChild<QPlainTextEdit *>(QStringLiteral("metadataSubject"));
        metadataDialogValid = title && title->text() == QStringLiteral("Updated title")
            && subject && subject->toPlainText() == QStringLiteral("First line\nSecond line\nThird line");
        dialog->reject();
    });
    metadataButton->click();
    if (!metadataDialogValid) return 92;
    window.show();
    app.processEvents();
    auto *scene = window.findChild<QGraphicsScene *>();
    auto *view = window.findChild<QGraphicsView *>();
    auto *tools = window.findChild<QWidget *>(QStringLiteral("textTools"));
    if (!scene || !view || !tools || !tools->isHidden()) return 8;
    auto *sidebar = window.findChild<QWidget *>(QStringLiteral("sidebar"));
    auto *toggle = window.findChild<QPushButton *>(QStringLiteral("sidebarToggle"));
    if (!sidebar || !toggle || !sidebar->isVisible()) return 21;
    if (!zoomIn->isEnabled() || !zoomOut->isEnabled()
        || zoomBubble->parentWidget() != view
        || zoomIn->y() >= zoomOut->y()) return 61;
    auto wheel = [&](int delta, Qt::KeyboardModifiers modifiers) {
        const QPointF pos = view->viewport()->rect().center();
        QWheelEvent event(pos, view->viewport()->mapToGlobal(pos.toPoint()),
                          QPoint(), QPoint(0, delta), Qt::NoButton, modifiers,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(view->viewport(), &event);
        app.processEvents();
    };
    wheel(60, Qt::ControlModifier);
    if (std::abs(view->transform().m11() - 1.0) > 0.0001) return 71;
    wheel(60, Qt::ControlModifier);
    if (std::abs(view->transform().m11() - 1.25) > 0.0001) return 72;
    wheel(-120, Qt::ControlModifier);
    if (std::abs(view->transform().m11() - 1.0) > 0.0001) return 73;
    const int scrollBefore = view->verticalScrollBar()->value();
    wheel(-120, Qt::NoModifier);
    if (std::abs(view->transform().m11() - 1.0) > 0.0001
        || view->verticalScrollBar()->value() <= scrollBefore) return 74;
    QGraphicsPixmapItem *pageImage = nullptr;
    for (auto *candidate : scene->items())
        if (auto *pixmap = qgraphicsitem_cast<QGraphicsPixmapItem *>(candidate)) pageImage = pixmap;
    if (!pageImage) return 67;
    const int originalPixels = pageImage->pixmap().width();
    const QRectF originalBounds = pageImage->sceneBoundingRect();
    zoomIn->click();
    if (std::abs(view->transform().m11() - 1.25) > 0.0001) return 62;
    if (pageImage->pixmap().width() <= originalPixels
        || std::abs(pageImage->sceneBoundingRect().width() - originalBounds.width()) > 0.01)
        return 68;
    app.processEvents();
    const QPoint bubblePosition = zoomBubble->pos();
    view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
    app.processEvents();
    if (zoomBubble->pos() != bubblePosition) return 69;
    view->verticalScrollBar()->setValue(view->verticalScrollBar()->minimum());
    app.processEvents();
    if (zoomBubble->pos() != bubblePosition) return 70;
    zoomOut->click();
    if (std::abs(view->transform().m11() - 1.0) > 0.0001) return 63;
    for (int i = 0; i < 20; ++i) zoomIn->click();
    if (zoomIn->isEnabled() || std::abs(view->transform().m11() - 4.0) > 0.0001) return 64;
    for (int i = 0; i < 30; ++i) zoomOut->click();
    if (zoomOut->isEnabled() || std::abs(view->transform().m11() - 0.25) > 0.0001) return 65;
    view->resetTransform();
    zoomIn->click();
    zoomOut->click();
    app.processEvents();
    const int expandedWidth = view->width();
    toggle->click();
    app.processEvents();
    if (sidebar->isVisible() || view->width() <= expandedWidth
        || !toggle->isVisible() || toggle->isChecked()
        || toggle->accessibleName() != QStringLiteral("Show tools")) return 22;
    if (zoomBubble->x() + zoomBubble->width() != view->viewport()->geometry().right() + 1 - 16
        || zoomBubble->y() + zoomBubble->height() != view->viewport()->geometry().bottom() + 1 - 16)
        return 66;
    toggle->click();
    app.processEvents();
    if (!sidebar->isVisible() || view->width() != expandedWidth
        || !toggle->isChecked() || toggle->accessibleName() != QStringLiteral("Hide tools")) return 23;
    auto *pageControls = window.findChild<QWidget *>(QStringLiteral("pageControls"));
    if (!pageControls || window.menuWidget()
        || sidebar->mapTo(window.centralWidget(), QPoint()).y() != 0
        || sidebar->height() != window.centralWidget()->height()
        || pageControls->mapTo(window.centralWidget(), QPoint()).y() != 0
        || pageControls->mapTo(window.centralWidget(), QPoint()).x() != sidebar->width()
        || toggle->parentWidget() != pageControls || toggle->x() != 8
        || window.findChild<QWidget *>(QStringLiteral("sidebarRail"))
        || sidebar->findChild<QPushButton *>(QStringLiteral("nextButton"))
        || view->height() < window.height() - pageControls->height() - 30) return 24;
    QGraphicsTextItem *item = nullptr;
    for (QGraphicsItem *candidate : scene->items())
        if (auto *textItem = qgraphicsitem_cast<EditableText *>(candidate)) item = textItem;
    if (!item) return 9;
    auto previewOffset = [&](const QString &file) {
        QImage preview(900, 1200, QImage::Format_RGB888);
        preview.fill(Qt::white);
        QPainter painter(&preview);
        scene->render(&painter, QRectF(0, 0, 900, 1200), QRectF(0, 0, 900, 1200));
        painter.end();
        fz_context *compareCtx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
        pdf_document *compareDoc = pdf_open_document(compareCtx, QFile::encodeName(file).constData());
        pdf_page *comparePage = pdf_load_page(compareCtx, compareDoc, 0);
        fz_pixmap *pixels = fz_new_pixmap_from_page(compareCtx,
            reinterpret_cast<fz_page *>(comparePage), fz_scale(1.5, 1.5), fz_device_rgb(compareCtx), 0);
        QImage exported(fz_pixmap_samples(compareCtx, pixels), fz_pixmap_width(compareCtx, pixels),
                        fz_pixmap_height(compareCtx, pixels), fz_pixmap_stride(compareCtx, pixels), QImage::Format_RGB888);
        auto firstInk = [](const QImage &image) {
            for (int y = 75; y < 145; ++y)
                for (int x = 75; x < 450; ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (c.red() < 128 && c.green() < 128 && c.blue() < 128) return y;
                }
            return -1;
        };
        const int previewY = firstInk(preview);
        const int exportedY = firstInk(exported);
        const int offset = previewY < 0 || exportedY < 0 ? 100 : previewY - exportedY;
        fz_drop_pixmap(compareCtx, pixels);
        pdf_drop_page(compareCtx, comparePage);
        pdf_drop_document(compareCtx, compareDoc);
        fz_drop_context(compareCtx);
        return offset;
    };
    if (std::abs(previewOffset(output)) > 2) return 19;
    const QPointF start = view->mapFromScene(item->mapToScene(QPointF(30, -12)));
    const QPointF end = start + QPointF(35, 25);
    auto mouse = [&](QEvent::Type type, QPointF p, Qt::MouseButtons buttons) {
        QMouseEvent event(type, p, view->viewport()->mapToGlobal(p.toPoint()),
                          Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &event);
        app.processEvents();
    };
    // An unselected box has no active handle; click the text to select it.
    mouse(QEvent::MouseButtonPress, start, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, start, Qt::NoButton);
    if (item->isSelected() || !tools->isHidden()) return 18;
    const QPointF text = view->mapFromScene(item->mapToScene(QPointF(30, 12)));
    mouse(QEvent::MouseButtonPress, text, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, text, Qt::NoButton);
    if (!item->isSelected() || tools->isHidden()) return 10;
    toggle->click();
    app.processEvents();
    if (tools->isHidden() || tools->x() + tools->width() > view->viewport()->width()) return 25;
    toggle->click();
    app.processEvents();
    mouse(QEvent::MouseButtonPress, start, Qt::LeftButton);
    mouse(QEvent::MouseMove, end, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, end, Qt::NoButton);
    if (item->pos().x() < 70 || item->pos().y() < 95) return 11;
    size->setValue(18);
    if (item->font().pixelSize() != 27) return 13;
    scene->clearSelection();
    if (!tools->isHidden()) return 12;
    item->setSelected(true);
    if (tools->isHidden()) return 14;
    auto *remove = tools->findChild<QPushButton *>();
    if (!remove) return 15;
    remove->click();
    for (QGraphicsItem *candidate : scene->items())
        if (candidate->type() == EditableText::Type) return 16;
    if (!tools->isHidden()) return 17;
    auto *checkButton = window.findChild<QPushButton *>(QStringLiteral("checkButton"));
    auto *crossButton = window.findChild<QPushButton *>(QStringLiteral("crossButton"));
    if (!checkButton || !crossButton) return 26;
    checkButton->click();
    QGraphicsItem *mark = nullptr;
    for (QGraphicsItem *candidate : scene->items())
        if (candidate->type() == MarkItem::Type) mark = candidate;
    if (!mark || !mark->isSelected() || tools->isHidden()) return 27;
    mark->moveBy(30, 20);
    if (mark->pos().x() < 30) return 28;
    remove->click();
    if (!tools->isHidden()) return 29;
    crossButton->click();
    if (scene->selectedItems().isEmpty() || scene->selectedItems().first()->type() != MarkItem::Type) return 30;

    PdfDocument reopened;
    reopened.open(output);
    auto fields = reopened.fields(0);
    if (fields.size() != 1 || fields[0].text != QStringLiteral("Editable text")
        || fields[0].fontSize != 24) {
        std::cerr << "Saved text did not survive reopening\n";
        return 5;
    }
    reopened.save(dir.filePath("edited.pdf"),
                  {{0, {{{60, 90, 250, 50}, QStringLiteral("Changed"), 8}}}});
    PdfDocument edited;
    edited.open(dir.filePath("edited.pdf"));
    if (edited.fields(0).size() != 1 || edited.fields(0)[0].text != QStringLiteral("Changed")
        || edited.fields(0)[0].fontSize != 8 || edited.fields(0)[0].rect.x() != 60)
        return 6;
    // Deleting the last box must also remove its PDF annotation.
    edited.save(dir.filePath("deleted.pdf"), {{0, {}}});
    PdfDocument deleted;
    deleted.open(dir.filePath("deleted.pdf"));
    if (!deleted.fields(0).isEmpty()) return 7;

    // Check and cross strokes survive a PDF round trip at their intended centers.
    PdfDocument markPdf;
    markPdf.open(QString::fromUtf8(input));
    const QString markPath = dir.filePath("marks.pdf");
    markPdf.save(markPath, {}, {{0, {{OptionMark::Check, {100, 150}},
                                     {OptionMark::Cross, {160, 150}}}}});
    PdfDocument marked;
    marked.open(markPath);
    const auto marks = marked.marks(0);
    if (marks.size() != 2 || marks[0].kind != OptionMark::Check ||
        marks[1].kind != OptionMark::Cross ||
        std::abs(marks[0].center.x() - 100) > 2 ||
        std::abs(marks[1].center.y() - 150) > 2) return 31;
    fz_context *markCtx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *markDoc = pdf_open_document(markCtx, QFile::encodeName(markPath).constData());
    pdf_page *markPage = pdf_load_page(markCtx, markDoc, 0);
    fz_pixmap *markPixels = fz_new_pixmap_from_page(markCtx, reinterpret_cast<fz_page *>(markPage),
                                                     fz_scale(1.5, 1.5), fz_device_rgb(markCtx), 0);
    QImage markImage(fz_pixmap_samples(markCtx, markPixels), fz_pixmap_width(markCtx, markPixels),
                     fz_pixmap_height(markCtx, markPixels), fz_pixmap_stride(markCtx, markPixels), QImage::Format_RGB888);
    int ink = 0;
    for (int y = 135; y < 165; ++y)
        for (int x = 88; x < 112; ++x)
            if (markImage.pixelColor(x, y).red() < 128) ++ink;
    fz_drop_pixmap(markCtx, markPixels);
    pdf_drop_page(markCtx, markPage);
    pdf_drop_document(markCtx, markDoc);
    fz_drop_context(markCtx);
    if (ink < 10) return 33;
    const QString removedMarks = dir.filePath("removed-marks.pdf");
    marked.save(removedMarks, {}, {{0, {}}});
    PdfDocument emptyMarks;
    emptyMarks.open(removedMarks);
    if (!emptyMarks.marks(0).isEmpty()) return 32;

    // A light paper background is removed; the transparent, cropped image is
    // stored both as the reusable template and on its placed stamp annotation.
    QImage paper(120, 60, QImage::Format_RGB32);
    paper.fill(Qt::white);
    { QPainter painter(&paper); painter.setPen(QPen(Qt::black, 4));
      painter.drawLine(20, 30, 100, 30); }
    const QString jpegPath = dir.filePath("signature.jpg");
    fz_context *jpegCtx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_pixmap *jpegPix = fz_new_pixmap(jpegCtx, fz_device_rgb(jpegCtx), 120, 60, nullptr, 0);
    fz_clear_pixmap_with_value(jpegCtx, jpegPix, 255);
    for (int y = 28; y < 32; ++y)
        for (int x = 20; x < 100; ++x) {
            unsigned char *p = fz_pixmap_samples(jpegCtx, jpegPix) + y * fz_pixmap_stride(jpegCtx, jpegPix) + x * 3;
            p[0] = p[1] = p[2] = 0;
        }
    fz_save_pixmap_as_jpeg(jpegCtx, jpegPix, QFile::encodeName(jpegPath).constData(), 90);
    fz_drop_pixmap(jpegCtx, jpegPix);
    fz_drop_context(jpegCtx);
    if (extractSignature(loadSignatureSource(jpegPath)).isNull()) return 42;
    // Allow manual verification with a private photo without making it a test fixture.
    if (!qEnvironmentVariableIsEmpty("JPDF_DESK_TEST_SIGNATURE") &&
        extractSignature(loadSignatureSource(qEnvironmentVariable("JPDF_DESK_TEST_SIGNATURE"))).isNull()) return 43;
    const QImage inkImage = extractSignature(paper);
    if (inkImage.isNull() || inkImage.width() >= paper.width() ||
        inkImage.pixelColor(0, 0).alpha() != 0 || !extractSignature(QImage()).isNull()) return 34;
    QByteArray png;
    QBuffer pngBuffer(&png);
    pngBuffer.open(QIODevice::WriteOnly);
    inkImage.save(&pngBuffer, "PNG");
    PdfDocument signaturePdf;
    signaturePdf.open(QString::fromUtf8(input));
    const QString signaturePath = dir.filePath("signed.pdf");
    signaturePdf.save(signaturePath, {}, {}, {{0, {{{100, 200, 180, 40}, png}}}}, png);
    PdfDocument signedDocument;
    signedDocument.open(signaturePath);
    const auto signatures = signedDocument.signatures(0);
    if (signedDocument.signatureTemplate() != png || signatures.size() != 1 ||
        signatures[0].png != png || std::abs(signatures[0].rect.x() - 100) > 2) return 35;
    fz_context *signatureCtx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *signatureDoc = pdf_open_document(signatureCtx, QFile::encodeName(signaturePath).constData());
    pdf_page *signaturePage = pdf_load_page(signatureCtx, signatureDoc, 0);
    fz_pixmap *signaturePixels = fz_new_pixmap_from_page(signatureCtx,
        reinterpret_cast<fz_page *>(signaturePage), fz_scale(1.5, 1.5), fz_device_rgb(signatureCtx), 0);
    QImage signaturePreview(fz_pixmap_samples(signatureCtx, signaturePixels),
        fz_pixmap_width(signatureCtx, signaturePixels), fz_pixmap_height(signatureCtx, signaturePixels),
        fz_pixmap_stride(signatureCtx, signaturePixels), QImage::Format_RGB888);
    int signatureInk = 0;
    for (int y = 200; y < 240; ++y)
        for (int x = 100; x < 280; ++x)
            if (signaturePreview.pixelColor(x, y).red() < 128) ++signatureInk;
    fz_drop_pixmap(signatureCtx, signaturePixels);
    pdf_drop_page(signatureCtx, signaturePage);
    pdf_drop_document(signatureCtx, signatureDoc);
    fz_drop_context(signatureCtx);
    if (signatureInk < 30) return 37;
    window.openDocument(signaturePath);
    auto *placeSignature = window.findChild<QPushButton *>(QStringLiteral("placeSignatureButton"));
    if (!placeSignature || !placeSignature->isEnabled()) return 38;
    placeSignature->click();
    QGraphicsItem *placed = nullptr;
    for (QGraphicsItem *candidate : scene->items())
        if (candidate->type() == SignatureItem::Type && candidate->isSelected()) placed = candidate;
    auto *widthControl = window.findChild<QSpinBox *>(QStringLiteral("signatureWidth"));
    if (!placed || !widthControl || !widthControl->isVisible()) return 39;
    // The blank corner of a transparent signature must be draggable too,
    // not just the opaque ink or a thin selection border.
    const QPointF blank = placed->mapToScene(QPointF(1, 1));
    if (scene->itemAt(blank, QTransform()) != placed) return 44;
    const QPointF dragStart = view->mapFromScene(blank);
    const QPointF oldPosition = placed->pos();
    mouse(QEvent::MouseButtonPress, dragStart, Qt::LeftButton);
    mouse(QEvent::MouseMove, dragStart + QPointF(25, 15), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, dragStart + QPointF(25, 15), Qt::NoButton);
    if (placed->pos().x() < oldPosition.x() + 20 || placed->pos().y() < oldPosition.y() + 10) return 45;
    const qreal oldWidth = placed->sceneBoundingRect().width();
    widthControl->setValue(220);
    if (placed->sceneBoundingRect().width() <= oldWidth) return 40;
    remove->click();
    signedDocument.save(dir.filePath("unsigned.pdf"), {}, {}, {{0, {}}});
    if (!signedDocument.signatures(0).isEmpty() || signedDocument.signatureTemplate() != png) return 36;

    // Check alignment at the default font size as well as the larger size.
    PdfDocument small;
    small.open(QString::fromUtf8(input));
    const QString smallPath = dir.filePath("small.pdf");
    small.save(smallPath, {{0, {{{50, 80, 250, 50}, QStringLiteral("Editable text"), 12}}}});
    window.openDocument(smallPath);
    if (std::abs(previewOffset(smallPath)) > 2) return 20;
    // Continuous pages retain edits/deletions and use page-local coordinates.
    const QString multiPath = dir.filePath("multiple.pdf");
    fz_context *multiCtx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *multiDoc = pdf_create_document(multiCtx);
    for (int i = 0; i < 2; ++i) {
        pdf_obj *multiPage = pdf_add_page(multiCtx, multiDoc, {0, 0, 600, 800}, 0, nullptr, nullptr);
        pdf_insert_page(multiCtx, multiDoc, -1, multiPage);
        pdf_drop_obj(multiCtx, multiPage);
    }
    pdf_save_document(multiCtx, multiDoc, QFile::encodeName(multiPath).constData(), &pdf_default_write_options);
    pdf_drop_document(multiCtx, multiDoc);
    fz_drop_context(multiCtx);
    PdfDocument multi;
    multi.open(multiPath);
    const QString multiEditedPath = dir.filePath("multiple-edited.pdf");
    multi.save(multiEditedPath, {{0, {{{10, 20, 200, 40}, QStringLiteral("Original"), 12}}}},
               {{0, {{OptionMark::Check, {80, 90}}}}}, {{0, {{{100, 200, 180, 40}, png}}}}, png);
    window.openDocument(multiEditedPath);
    auto *previousButton = window.findChild<QPushButton *>(QStringLiteral("previousButton"));
    auto *nextButton = window.findChild<QPushButton *>(QStringLiteral("nextButton"));
    if (!previousButton || !nextButton || previousButton->isEnabled() || !nextButton->isEnabled()) return 48;
    for (auto *candidate : scene->items()) {
        if (auto *textItem = qgraphicsitem_cast<EditableText *>(candidate)) textItem->setPlainText(QStringLiteral("Unsaved"));
        if (isAnnotation(candidate)) candidate->moveBy(15, 25);
    }
    QGraphicsItem *firstRoot = nullptr;
    QGraphicsItem *secondRoot = nullptr;
    for (auto *candidate : scene->items()) {
        if (!candidate->parentItem() && candidate->data(0).isValid()) {
            if (candidate->data(0).toInt() == 0) firstRoot = candidate;
            if (candidate->data(0).toInt() == 1) secondRoot = candidate;
        }
    }
    if (!firstRoot || !secondRoot || secondRoot->sceneBoundingRect().top() <= firstRoot->sceneBoundingRect().bottom()
        || scene->sceneRect().height() < 2400) return 75;
    nextButton->click();
    app.processEvents();
    if (!captureAnnotations(*scene, secondRoot).fields.isEmpty() || nextButton->isEnabled()) return 49;
    auto *pageLabel = window.findChild<QLabel *>(QStringLiteral("pageLabel"));
    if (!pageLabel || pageLabel->text() != QStringLiteral("Page 2 / 2")) return 76;
    auto *addButton = window.findChild<QPushButton *>(QStringLiteral("addButton"));
    addButton->click();
    auto secondAnnotations = captureAnnotations(*scene, secondRoot);
    if (secondAnnotations.fields.size() != 1 || secondAnnotations.fields[0].rect.y() >= 1200
        || secondAnnotations.fields[0].rect.y() < 0) return 77;
    addAnnotations(*scene, {{}, {{OptionMark::Cross, {80, 90}}},
                            {{{100, 200, 180, 40}, png}}}, secondRoot);
    secondAnnotations = captureAnnotations(*scene, secondRoot);
    if (secondAnnotations.marks.size() != 1 || secondAnnotations.marks[0].center != QPointF(80, 90)
        || secondAnnotations.signatures.size() != 1
        || secondAnnotations.signatures[0].rect.topLeft() != QPointF(100, 200)) return 80;
    const auto firstAnnotations = captureAnnotations(*scene, firstRoot);
    const QString continuousPath = dir.filePath("continuous-saved.pdf");
    multi.save(continuousPath,
               {{0, firstAnnotations.fields}, {1, secondAnnotations.fields}},
               {{0, firstAnnotations.marks}, {1, secondAnnotations.marks}},
               {{0, firstAnnotations.signatures}, {1, secondAnnotations.signatures}}, png);
    PdfDocument continuousSaved;
    continuousSaved.open(continuousPath);
    if (continuousSaved.fields(0).first().text != QStringLiteral("Unsaved")
        || continuousSaved.fields(1).size() != 1
        || std::abs(continuousSaved.marks(1).first().center.x() - 80) > 0.01
        || std::abs(continuousSaved.marks(1).first().center.y() - 90) > 0.01
        || std::abs(continuousSaved.signatures(1).first().rect.x() - 100) > 0.01
        || std::abs(continuousSaved.signatures(1).first().rect.y() - 200) > 0.01) return 81;
    for (auto *candidate : secondRoot->childItems())
        if (isAnnotation(candidate)) delete candidate;
    view->verticalScrollBar()->setValue(view->verticalScrollBar()->minimum());
    app.processEvents();
    if (pageLabel->text() != QStringLiteral("Page 1 / 2")) return 78;
    view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
    app.processEvents();
    if (pageLabel->text() != QStringLiteral("Page 2 / 2")) return 79;
    previousButton->click();
    const auto draft = captureAnnotations(*scene, firstRoot);
    if (draft.fields.size() != 1 || draft.fields[0].text != QStringLiteral("Unsaved") ||
        draft.fields[0].rect.topLeft() != QPointF(25, 45) || draft.marks.size() != 1 ||
        draft.marks[0].center != QPointF(95, 115) || draft.signatures.size() != 1 ||
        draft.signatures[0].png != png) return 50;
    for (auto *candidate : scene->items())
        if (isAnnotation(candidate)) delete candidate;
    nextButton->click();
    previousButton->click();
    const auto deletedDraft = captureAnnotations(*scene, firstRoot);
    if (!deletedDraft.fields.isEmpty() || !deletedDraft.marks.isEmpty() ||
        !deletedDraft.signatures.isEmpty()) return 51;

    // Scene conversion excludes backgrounds and preserves all annotation types.
    QGraphicsScene annotationScene;
    annotationScene.addRect(0, 0, 900, 1200);
    const PageAnnotations original{{{{10, 20, 200, 40}, QStringLiteral("Draft"), 18}},
                                   {{OptionMark::Cross, {80, 90}}},
                                   {{{100, 200, 180, 40}, png}}};
    addAnnotations(annotationScene, original);
    const auto captured = captureAnnotations(annotationScene);
    if (captured.fields.size() != 1 || captured.marks.size() != 1 ||
        captured.signatures.size() != 1 || captured.fields[0].text != QStringLiteral("Draft") ||
        captured.fields[0].fontSize != 18 || captured.fields[0].rect.topLeft() != QPointF(10, 20) ||
        captured.marks[0].center != QPointF(80, 90) || captured.signatures[0].png != png) return 46;
    for (auto *candidate : annotationScene.items())
        if (isAnnotation(candidate)) delete candidate;
    const auto empty = captureAnnotations(annotationScene);
    if (!empty.fields.isEmpty() || !empty.marks.isEmpty() || !empty.signatures.isEmpty()) return 47;
    return 0;
}
