#include "main_window.h"
#include "document_view.h"
#include "ui/page_annotations.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

namespace {
void makePdf(const QString &path)
{
    auto *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    auto *doc = pdf_create_document(ctx);
    auto *page = pdf_add_page(ctx, doc, {0, 0, 200, 300}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, doc, -1, page);
    pdf_drop_obj(ctx, page);
    pdf_save_document(ctx, doc, path.toUtf8().constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}
}

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString input = dir.filePath("input.pdf"), second = dir.filePath("second.pdf");
    makePdf(input);
    makePdf(second);
    // The input contains a legacy reusable template. A UI save must strip it,
    // even if it is selected as the fallback signature image.
    QImage image(20, 10, QImage::Format_RGB32);
    image.fill(Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    PdfDocument fixture;
    fixture.open(input);
    const QString legacy = dir.filePath("legacy.pdf");
    fixture.saveSnapshot(legacy, {}, png);

    MainWindow window;
    window.openDocument(legacy);
    window.show();
    auto *view = window.findChild<DocumentView *>();
    if (!view || view->hasUnsavedChanges() || !window.close()) return 2;
    window.show();
    view->addText();
    const auto original = view->captureDrafts();
    const auto title = window.windowTitle();
    int prompts = 0, errors = 0;
    const auto respond = [&](QMessageBox::StandardButton answer, const QString &savePath,
                             const std::function<void()> &action) {
        prompts = errors = 0;
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, &window, [&] {
            auto *active = QApplication::activeModalWidget();
            if (auto *box = qobject_cast<QMessageBox *>(active)) {
                if (box->objectName() == QStringLiteral("unsavedChangesDialog")) {
                    ++prompts;
                    box->done(answer);
                } else {
                    ++errors;
                    box->accept();
                }
            } else if (auto *dialog = qobject_cast<QFileDialog *>(active)) {
                if (savePath.isEmpty()) dialog->reject();
                else {
                    dialog->setOption(QFileDialog::DontConfirmOverwrite);
                    dialog->selectFile(savePath);
                    QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
                }
            }
        });
        timer.start(5);
        action();
    };
    bool closed = false;
    respond(QMessageBox::Cancel, {}, [&] { closed = window.close(); });
    if (closed || prompts != 1 || !(view->captureDrafts() == original)) return 3;
    respond(QMessageBox::Cancel, {}, [&] { window.openDocument(second); });
    if (prompts != 1 || window.windowTitle() != title || !(view->captureDrafts() == original)) return 4;
    respond(QMessageBox::Save, {}, [&] { window.openDocument(second); });
    if (prompts != 1 || window.windowTitle() != title || !view->hasUnsavedChanges()) return 5;
    // Failed saves must not authorize closing/replacing a dirty document.
    respond(QMessageBox::Save, legacy, [&] { window.openDocument(second); });
    if (prompts != 1 || errors != 1 || window.windowTitle() != title || !view->hasUnsavedChanges()) return 6;
    const QString output = dir.filePath("saved.pdf");
    respond(QMessageBox::Save, output, [&] { window.openDocument(second); });
    if (prompts != 1 || errors || view->hasUnsavedChanges() ||
        !window.windowTitle().contains(QStringLiteral("second.pdf"))) return 7;
    PdfDocument saved;
    saved.open(output);
    if (saved.fields(0).size() != 1 || !saved.signatureTemplate().isEmpty()) return 8;

    // Metadata-only changes must be protected, even with no scene annotations.
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("metadataDialog"));
        if (!dialog) return;
        dialog->findChild<QLineEdit *>(QStringLiteral("metadataTitle"))->setText("Changed");
        dialog->accept();
    });
    window.findChild<QAction *>(QStringLiteral("metadataButton"))->trigger();
    respond(QMessageBox::Cancel, {}, [&] { closed = window.close(); });
    if (closed || prompts != 1 || view->hasUnsavedChanges()) return 9;
    respond(QMessageBox::Discard, {}, [&] { window.openDocument(second); });
    if (prompts != 1) return 10;
    // Password-only changes are in-memory edits as well.
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("passwordDialog"));
        if (!dialog) return;
        dialog->findChild<QLineEdit *>(QStringLiteral("newPassword"))->setText("secret");
        dialog->findChild<QLineEdit *>(QStringLiteral("confirmPassword"))->setText("secret");
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QAction *>(QStringLiteral("passwordButton"))->trigger();
    respond(QMessageBox::Cancel, {}, [&] { closed = window.close(); });
    if (closed || prompts != 1) return 11;
    respond(QMessageBox::Discard, {}, [&] { closed = window.close(); });
    if (!closed || prompts != 1) return 12;
    return 0;
}
