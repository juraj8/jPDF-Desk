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
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>

namespace {
class TaskVerifier final : public VerificationProvider {
public:
    mutable std::atomic<bool> entered{false}, released{false};
    mutable bool workerThread = false;
    pdf_pkcs7_verifier *createVerifier(fz_context *) const override
    {
        workerThread = QThread::currentThread() != qApp->thread();
        entered.store(true);
        while (!released.load()) QThread::msleep(1);
        auto *verifier = new pdf_pkcs7_verifier{};
        verifier->drop = [](fz_context *, pdf_pkcs7_verifier *handle) { delete handle; };
        return verifier;
    }
    QString trustDescription() const override { return QStringLiteral("Test trust policy"); }
};

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
    QTimer::singleShot(0, [&] {
        if (auto *prompt = window.findChild<QMessageBox *>("unsavedChangesDialog")) {
            prompt->done(QMessageBox::Discard);
            QTimer::singleShot(0, cancelTask);
        }
    });
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

    // Offline verification must leave the GUI event loop running, and cancel
    // must not publish a new report. The fake verifier waits for a GUI event.
    auto verifier = std::make_shared<TaskVerifier>();
    MainWindow verificationWindow(nullptr, {{}, verifier});
    verificationWindow.openDocument(path);
    auto *verify = verificationWindow.findChild<QPushButton *>("verifySignaturesButton");
    bool sawReport = false, cancel = false, sawCancellation = false;
    QTimer verifyTimer;
    QObject::connect(&verifyTimer, &QTimer::timeout, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) return;
        if (dialog->objectName() == "pdfTaskDialog" && verifier->entered.load()) {
            if (cancel) { dialog->reject(); sawCancellation = true; }
            verifier->released.store(true);
        } else if (dialog->objectName() == "signatureCheckDialog") {
            sawReport = true;
            dialog->accept();
        }
    });
    verifyTimer.start(1);
    verify->click();
    if (!sawReport || !verifier->workerThread) return 14;
    verifier->entered.store(false);
    verifier->released.store(false);
    sawReport = false;
    cancel = true;
    verify->click();
    verifyTimer.stop();
    if (!sawCancellation || sawReport || !verifier->workerThread) return 15;
    return 0;
}
