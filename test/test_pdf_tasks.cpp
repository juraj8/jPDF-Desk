#include "document_loading.h"
#include "document_view.h"
#include "main_window.h"
#include "pdf_task.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>

namespace {
void makePdf(const QString &path)
{
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    auto *resources = pdf_new_dict(ctx, doc, 1);
    auto *fonts = pdf_dict_put_dict(ctx, resources, PDF_NAME(Font), 1);
    auto *font = pdf_new_dict(ctx, doc, 3);
    pdf_dict_puts(ctx, fonts, "F1", font);
    pdf_dict_put(ctx, font, PDF_NAME(Type), PDF_NAME(Font));
    pdf_dict_put(ctx, font, PDF_NAME(Subtype), PDF_NAME(Type1));
    pdf_dict_put_name(ctx, font, PDF_NAME(BaseFont), "Helvetica");
    const QByteArray contents("BT /F1 12 Tf 30 300 Td (Needle) Tj ET");
    for (int i = 0; i < 40; ++i) {
        auto *buffer = fz_new_buffer_from_copied_data(ctx,
            reinterpret_cast<const unsigned char *>(contents.constData()), contents.size());
        auto *page = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, resources, buffer);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
        fz_drop_buffer(ctx, buffer);
    }
    pdf_drop_obj(ctx, font);
    pdf_drop_obj(ctx, resources);
    pdf_save_document(ctx, doc, QFile::encodeName(path).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}
void cancelTask()
{
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    if (dialog && dialog->objectName() == "pdfTaskDialog") dialog->reject();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    int ticks = 0;
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; });
    heartbeat.start(1);
    bool workerThread = false;
    // The worker waits for an event delivered on the GUI thread: this would
    // deadlock if runPdfTask performed its work on that thread.
    std::atomic<bool> released{false};
    QTimer::singleShot(20, [&] { released.store(true); });
    if (!runPdfTask(nullptr, "Test work", [&](const PdfTaskProgress &progress) {
        workerThread = QThread::currentThread() != app.thread();
        while (!released.load()) { progress.checkCancelled(); QThread::msleep(1); }
        progress.report(1, 1);
    }) || !workerThread || ticks == 0) return 1;

    bool stopped = false;
    QTimer::singleShot(10, cancelTask);
    if (runPdfTask(nullptr, "Cancel work", [&](const PdfTaskProgress &progress) {
        while (!progress.cancelled()) QThread::msleep(1);
        stopped = true;
        progress.checkCancelled();
    }) || !stopped) return 2;
    try {
        runPdfTask(nullptr, "Error work", [](const auto &) { throw std::runtime_error("Worker error"); });
        return 3;
    } catch (const std::runtime_error &e) {
        if (QString::fromUtf8(e.what()) != "Worker error") return 4;
    }

    QTemporaryDir dir;
    const QString path = dir.filePath("source.pdf");
    makePdf(path);
    LoadedPdf loaded;
    if (!runPdfTask(nullptr, "Load", [&](const auto &progress) {
        loaded = loadPdf({path, {}}, 1, progress);
    }) || loaded.pages.size() != 40 || loaded.firstPage.isNull() || loaded.thumbnails.size() != 4) return 5;
    PdfDocument doc;
    doc.swapContent(*loaded.document);
    if (doc.pageCount() != 40 || doc.path() != path || loaded.document->pageCount() != 0) return 6;
    int calls = 0;
    try {
        doc.search("Needle", [&](int completed, int total) {
            if (total != 40 || completed != calls++) throw std::runtime_error("Bad progress");
            return completed < 2;
        });
        return 7;
    } catch (const PdfOperationCancelled &) {}
    if (calls != 3 || doc.search("Needle").size() != 40) return 8;

    doc.setPassword("secret");
    const QString encrypted = dir.filePath("encrypted.pdf");
    doc.saveSnapshot(encrypted, {});
    const auto source = doc.savedSource();
    QVector<TextSearchMatch> matches;
    if (!runPdfTask(nullptr, "Encrypted search", [&](const auto &progress) {
        PdfDocument independent;
        independent.open(source.path, source.password);
        matches = independent.search("Needle", [&](int completed, int total) {
            progress.report(completed, total);
            return true;
        });
    }) || matches.size() != 40) return 9;
    PdfTaskProgress cancelled;
    cancelled.cancel();
    try { loadPdf({path, {}}, 1, cancelled); return 10; } catch (const PdfOperationCancelled &) {}

    MainWindow window;
    window.openDocument(path);
    auto *view = window.findChild<DocumentView *>();
    view->addText();
    const QString title = window.windowTitle();
    QTimer::singleShot(0, cancelTask);
    window.openDocument(path);
    if (window.windowTitle() != title || view->captureDrafts().fields[0].size() != 1) return 11;
    auto *query = window.findChild<QLineEdit *>("searchQuery");
    auto *count = window.findChild<QLabel *>("searchResultCount");
    query->setText("Needle");
    QTimer::singleShot(0, cancelTask);
    query->returnPressed();
    if (!count->text().isEmpty() || view->captureDrafts().fields[0].size() != 1) return 12;
    query->returnPressed();
    if (count->text() != "1 / 40") return 13; // No stale cancellation/results.
    return 0;
}
