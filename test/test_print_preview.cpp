#include "ui/print_preview.h"

#include <QApplication>
#include <QGraphicsView>
#include <QPainter>
#include <QPrintPreviewWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QWheelEvent>
#include <stdexcept>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    int renders = 0;
    double renderedScale = 0;
    QString error;
    PrintPreview preview([&](QPrinter &printer, const PrintOptions &options) {
        ++renders;
        renderedScale = options.scalePercent;
        QPainter painter(&printer);
        painter.drawText(100, 100, QStringLiteral("First page"));
        printer.newPage();
        painter.drawText(100, 100, QStringLiteral("Second page"));
    }, [&](const QString &message) { error = message; });
    PrintSettings settings;
    settings.options.scalePercent = 80;
    settings.copies = 5;
    if (!preview.configure(settings, 8, 3).isEmpty()) return 1;
    preview.refresh();
    preview.show();
    app.processEvents();
    auto *view = preview.findChild<QPrintPreviewWidget *>(QStringLiteral("printPreview"));
    auto *page = preview.findChild<QSpinBox *>(QStringLiteral("printPreviewPage"));
    auto *zoom = preview.findChild<QSpinBox *>(QStringLiteral("printPreviewZoom"));
    if (!view || !page || !zoom || view->pageCount() != 2 || page->maximum() != 2
        || renders != 1 || renderedScale != 80 || !error.isEmpty()) return 2;
    page->setValue(2);
    if (view->currentPage() != 2) return 3;
    page->setValue(1);
    if (view->currentPage() != 1) return 4;
    zoom->setValue(150);
    if (qAbs(view->zoomFactor() - 1.5) > 0.001) return 5;
    auto *graphics = view->findChild<QGraphicsView *>();
    if (!graphics) return 6;
    QWheelEvent wheel(QPointF(50, 50), QPointF(50, 50), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(graphics->viewport(), &wheel);
    if (qAbs(view->zoomFactor() - 1.8) > 0.001 || zoom->value() != 180) return 7;
    zoom->setValue(1000);
    QWheelEvent upperLimit(QPointF(50, 50), QPointF(50, 50), QPoint(), QPoint(0, 120),
                           Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(graphics->viewport(), &upperLimit);
    if (qAbs(view->zoomFactor() - 10) > 0.001 || zoom->value() != 1000) return 8;
    preview.findChild<QPushButton *>(QStringLiteral("printPreviewFitPage"))->click();
    if (view->zoomMode() != QPrintPreviewWidget::FitInView) return 9;
    preview.findChild<QPushButton *>(QStringLiteral("printPreviewFitWidth"))->click();
    if (view->zoomMode() != QPrintPreviewWidget::FitToWidth || renders != 1) return 10;
    settings.range = QPrinter::PageRange;
    settings.pageRange = QStringLiteral("99");
    settings.options.scalePercent = 90;
    if (preview.configure(settings, 8, 3).isEmpty()) return 11;
    // A rejected snapshot does not replace the previous rendering options.
    preview.refresh();
    if (renderedScale != 80 || renders != 2) return 12;
    settings.range = QPrinter::AllPages;
    settings.options.scalePercent = 120;
    if (!preview.configure(settings, 8, 3).isEmpty()) return 13;
    preview.refresh();
    if (renderedScale != 120 || renders != 3) return 14;
    PrintPreview failing([](QPrinter &, const PrintOptions &) {
        throw std::runtime_error("render failed");
    }, [&](const QString &message) { error = message; });
    if (!failing.configure(settings, 8, 3).isEmpty()) return 15;
    failing.refresh();
    if (!error.contains(QStringLiteral("render failed"))) return 16;
    return 0;
}
