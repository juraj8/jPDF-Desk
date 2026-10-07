#include "ui/print_dialog.h"
#include "jpdf_desk/printing/pdf_printing.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPrinter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <QScrollArea>
#include <QTabBar>
#include <QTreeWidget>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QPainter>
#include <QPrintPreviewWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QGraphicsView>
#include <QWheelEvent>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QPrinter printer;
    PrintDialog dialog(printer, 8, 3);
    const auto cards = dialog.findChildren<QFrame *>(QStringLiteral("printCard"));
    if (cards.size() != 10) return 12;
    auto *views = dialog.findChild<QStackedWidget *>(QStringLiteral("printViews"));
    auto *switches = dialog.findChild<QTabBar *>(QStringLiteral("printViewCards"));
    if (!views || views->count() != 5 || !switches || switches->count() != 5
        || switches->currentIndex() != 0 || views->currentIndex() != 0) return 14;
    auto *documentView = qobject_cast<QScrollArea *>(views->widget(0));
    if (!documentView) return 13;
    auto *grid = qobject_cast<QGridLayout *>(documentView->widget()->layout());
    if (!grid || !grid->itemAtPosition(0, 0) || !grid->itemAtPosition(0, 1)
        || grid->itemAtPosition(0, 0)->widget() != grid->itemAtPosition(0, 1)->widget()
        || !grid->itemAtPosition(1, 0) || !grid->itemAtPosition(1, 1)) return 13;
    auto *destination = dialog.findChild<QTreeWidget *>(QStringLiteral("printDestination"));
    auto *range = dialog.findChild<QButtonGroup *>(QStringLiteral("printRange"));
    auto *pages = dialog.findChild<QLineEdit *>(QStringLiteral("printPageRange"));
    auto *output = dialog.findChild<QLineEdit *>(QStringLiteral("printOutputPath"));
    auto *confirm = dialog.findChild<QPushButton *>(QStringLiteral("confirmPrint"));
    auto *error = dialog.findChild<QLabel *>(QStringLiteral("printError"));
    if (!destination || !range || !pages || !output || !confirm || !error) return 2;
    if (confirm->text() != QStringLiteral("Print")) return 17;
    destination->setCurrentItem(destination->topLevelItem(0));
    output->setText(directory.filePath(QStringLiteral("output.pdf")));
    range->button(QPrinter::PageRange)->click();
    for (const auto &invalid : {QString(), QStringLiteral("9"), QStringLiteral("abc")}) {
        pages->setText(invalid);
        confirm->click();
        if (dialog.result() == QDialog::Accepted || error->isHidden()) return 3;
        if (printer.printRange() != QPrinter::AllPages) return 4;
    }
    auto *colour = dialog.findChild<QComboBox *>(QStringLiteral("printColour"));
    auto *resolution = dialog.findChild<QComboBox *>(QStringLiteral("printResolution"));
    auto *duplex = dialog.findChild<QComboBox *>(QStringLiteral("printDuplex"));
    if (!colour || !resolution || !duplex || colour->count() != 2 || duplex->isEnabled()) return 10;
    colour->setCurrentIndex(colour->findData(QPrinter::GrayScale));
    resolution->setCurrentIndex(resolution->findData(150));
    for (int i = 1; i < 5; ++i) {
        switches->setCurrentIndex(i);
        if (views->currentIndex() != i) return 15;
    }
    switches->setCurrentIndex(0);
    if (views->currentIndex() != 0 || colour->currentData().toInt() != QPrinter::GrayScale
        || resolution->currentData().toInt() != 150 || output->text().isEmpty()) return 16;
    auto *perSheet = dialog.findChild<QComboBox *>(QStringLiteral("printPagesPerSheet"));
    auto *scale = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("printScale"));
    if (!perSheet || !scale) return 18;
    perSheet->setCurrentIndex(perSheet->findData(2));
    scale->setValue(80);
    pages->setText(QStringLiteral("1-3, 5, 8"));
    QFile existing(output->text());
    if (!existing.open(QIODevice::WriteOnly)) return 5;
    existing.close();
    confirm->click();
    if (dialog.result() == QDialog::Accepted) return 6;
    existing.remove();
    confirm->click();
    if (dialog.result() != QDialog::Accepted || printer.outputFormat() != QPrinter::PdfFormat) return 7;
    if (selectedPrintPages(printer, 8, 3) != QVector<int>({0, 1, 2, 4, 7})) return 8;
    if (printer.colorMode() != QPrinter::GrayScale || printer.resolution() != 150) return 11;
    if (dialog.options().pagesPerSheet != 2 || dialog.options().scalePercent != 80) return 19;
    QPrinter currentPrinter;
    PrintDialog current(currentPrinter, 8, 3);
    auto *currentDestination = current.findChild<QTreeWidget *>(QStringLiteral("printDestination"));
    currentDestination->setCurrentItem(currentDestination->topLevelItem(0));
    current.findChild<QLineEdit *>(QStringLiteral("printOutputPath"))->setText(directory.filePath(QStringLiteral("current.pdf")));
    current.findChild<QButtonGroup *>(QStringLiteral("printRange"))->button(QPrinter::CurrentPage)->click();
    current.findChild<QPushButton *>(QStringLiteral("confirmPrint"))->click();
    if (current.result() != QDialog::Accepted || selectedPrintPages(currentPrinter, 8, 3) != QVector<int>({3})) return 9;
    // Preview works without an output filename, uses a separate printer, and stays inline.
    QPrinter untouched;
    int renderCount = 0;
    PrintDialog previewDialog(untouched, 8, 3, nullptr,
        [&renderCount](QPrinter &target, const PrintOptions &options) {
            if (options.pagesPerSheet != 1) return;
            ++renderCount;
            QPainter painter(&target);
            painter.drawText(100, 100, QStringLiteral("Preview"));
        });
    auto *previewDestination = previewDialog.findChild<QTreeWidget *>(QStringLiteral("printDestination"));
    previewDestination->setCurrentItem(previewDestination->topLevelItem(0));
    auto *previewButton = previewDialog.findChild<QPushButton *>(QStringLiteral("printPreviewButton"));
    previewButton->click();
    auto *previewView = previewDialog.findChild<QPrintPreviewWidget *>(QStringLiteral("printPreview"));
    if (!previewView || previewView->isHidden() || renderCount != 1 || previewView->pageCount() != 1
        || !untouched.outputFileName().isEmpty() || previewDialog.result() == QDialog::Accepted) return 20;
    auto *zoom = previewDialog.findChild<QSpinBox *>(QStringLiteral("printPreviewZoom"));
    auto *graphics = previewView->findChild<QGraphicsView *>();
    if (!zoom || !graphics) return 22;
    zoom->setValue(150);
    if (qAbs(previewView->zoomFactor() - 1.5) > 0.001) return 23;
    QWheelEvent wheel(QPointF(50, 50), QPointF(50, 50), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(graphics->viewport(), &wheel);
    if (qAbs(previewView->zoomFactor() - 1.8) > 0.001 || zoom->value() != 180) return 24;
    // Preview zoom must not alter the job's physical print scaling.
    if (previewDialog.options().scalePercent != 100 || renderCount != 1) return 25;
    previewDialog.findChild<QPushButton *>(QStringLiteral("printPreviewFitPage"))->click();
    if (previewView->zoomMode() != QPrintPreviewWidget::FitInView) return 26;
    previewDialog.findChild<QPushButton *>(QStringLiteral("printPreviewFitWidth"))->click();
    if (previewView->zoomMode() != QPrintPreviewWidget::FitToWidth) return 27;
    previewButton->click();
    if (!previewView->isHidden() || previewButton->text() != QStringLiteral("Preview")) return 21;
    return 0;
}
