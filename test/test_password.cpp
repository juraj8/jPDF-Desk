#include "main_window.h"
#include "pdf_filler/document/pdf_document.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QInputDialog>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <cstring>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    const QString plain = dir.filePath("plain.pdf");
    const QString protectedPath = dir.filePath("protected.pdf");
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    const char contents[] = "0 0 0 rg 10 10 100 100 re f";
    fz_buffer *buffer = fz_new_buffer_from_copied_data(ctx,
        reinterpret_cast<const unsigned char *>(contents), sizeof(contents) - 1);
    pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 300, 400}, 0, nullptr, buffer);
    pdf_insert_page(ctx, doc, -1, page);
    pdf_drop_obj(ctx, page);
    fz_drop_buffer(ctx, buffer);
    pdf_save_document(ctx, doc, QFile::encodeName(plain).constData(), &pdf_default_write_options);
    pdf_write_options options = pdf_default_write_options;
    options.do_encrypt = PDF_ENCRYPT_AES_256;
    options.do_compress = 1;
    std::strcpy(options.upwd_utf8, "secret");
    std::strcpy(options.opwd_utf8, "owner-secret");
    pdf_save_document(ctx, doc, QFile::encodeName(protectedPath).constData(), &options);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);

    PdfDocument pdf;
    pdf.open(plain);
    for (const QString &password : {QString(), QString("wrong")}) {
        try {
            pdf.open(protectedPath, password);
            return 2;
        } catch (const PdfPasswordRequired &) {}
        if (pdf.path() != plain || pdf.pageCount() != 1) return 3;
    }
    pdf.open(protectedPath, "secret");
    if (pdf.pageCount() != 1 || pdf.render(0).isNull()) return 4;
    pdf.setMetadata({{"Title", "Encrypted document"}});
    const QString saved = dir.filePath("saved.pdf");
    pdf.save(saved, {});
    if (pdf.path() != saved || pdf.render(0).isNull()) return 5;
    PdfDocument reopened;
    try {
        reopened.open(saved);
        return 6; // Saving must preserve encryption.
    } catch (const PdfPasswordRequired &) {}
    reopened.open(saved, "secret");
    if (reopened.metadata().value("Title") != "Encrypted document") return 7;
    reopened.open(protectedPath, "owner-secret");
    if (reopened.render(0).isNull()) return 8;

    // Set, replace, and clear encryption on saved copies.
    reopened.open(plain);
    reopened.setPassword("first-password");
    const QString first = dir.filePath("first.pdf");
    reopened.save(first, {});
    PdfDocument reader;
    reader.open(first, "first-password");
    reopened.setPassword("replacement");
    const QString replacement = dir.filePath("replacement.pdf");
    reopened.save(replacement, {});
    try {
        reader.open(replacement, "first-password");
        return 11;
    } catch (const PdfPasswordRequired &) {}
    reader.open(replacement, "replacement");
    reopened.setPassword({});
    const QString cleared = dir.filePath("cleared.pdf");
    reopened.save(cleared, {});
    reader.open(cleared);
    if (reader.render(0).isNull()) return 12;
    reader.open(first, "first-password"); // Earlier copies are unchanged.
    for (const QString &invalid : {QString(128, 'x'), QString("a") + QChar(0)}) {
        try {
            reader.setPassword(invalid);
            return 13;
        } catch (const std::runtime_error &) {}
    }
    // Opening another file discards a pending password change.
    reader.setPassword("discarded");
    reader.open(plain);
    reader.save(dir.filePath("reset.pdf"), {});
    reopened.open(dir.filePath("reset.pdf"));

    MainWindow window;
    auto *passwordButton = window.findChild<QPushButton *>("passwordButton");
    if (!passwordButton || passwordButton->isEnabled()) return 14;
    window.openDocument(plain);
    if (!passwordButton->isEnabled()) return 15;
    bool validated = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>("passwordDialog");
        if (!dialog) return;
        auto *password = dialog->findChild<QLineEdit *>("newPassword");
        auto *confirm = dialog->findChild<QLineEdit *>("confirmPassword");
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        password->setText("new-secret");
        confirm->setText("mismatch");
        buttons->button(QDialogButtonBox::Ok)->click();
        validated = dialog->isVisible() && !dialog->findChild<QLabel *>("passwordError")->text().isEmpty()
            && password->echoMode() == QLineEdit::Password && confirm->echoMode() == QLineEdit::Password;
        confirm->setText("new-secret");
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    passwordButton->click();
    if (!validated) return 16;
    QTimer::singleShot(0, [&] {
        auto *dialog = window.findChild<QDialog *>("passwordDialog");
        dialog->findChild<QCheckBox *>("clearPassword")->setChecked(true);
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    passwordButton->click();
    const QString previousTitle = window.windowTitle();
    int prompts = 0;
    bool masked = true;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        if (!dialog) return;
        masked = masked && dialog->textEchoMode() == QLineEdit::Password;
        dialog->setTextValue(prompts++ == 0 ? "wrong" : "secret");
        dialog->accept();
    });
    timer.start(10);
    window.openDocument(protectedPath);
    timer.stop();
    if (prompts != 2 || !masked || window.windowTitle() == previousTitle) return 9;
    const QString encryptedTitle = window.windowTitle();
    QObject::disconnect(&timer, nullptr, nullptr, nullptr);
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget()))
            dialog->reject();
    });
    timer.start(10);
    window.openDocument(protectedPath);
    timer.stop();
    if (window.windowTitle() != encryptedTitle) return 10;
    return 0;
}
