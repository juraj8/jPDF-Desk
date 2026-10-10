#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/signing/openssl_provider.h"
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    auto run = [&](const QStringList &args) {
        QProcess process;
        process.start(QString::fromLocal8Bit(argv[1]), args);
        const bool success = process.waitForFinished(30000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!success)
            std::cerr << "Certificate generation failed: " << argv[1] << '\n'
                      << process.readAllStandardError().constData() << '\n';
        return success;
    };
    const QString key = dir.filePath("key.pem"), cert = dir.filePath("cert.pem");
    const QString pfx = dir.filePath("cert.p12");
    if (!run({"req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", key,
              "-out", cert, "-subj", "/CN=jPDF Desk Test", "-days", "1"}) ||
        !run({"pkcs12", "-export", "-inkey", key, "-in", cert, "-out", pfx,
              "-passout", "pass:test-password"})) return 2;
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *doc = pdf_create_document(ctx);
    pdf_obj *page = pdf_add_page(ctx, doc, {0, 0, 600, 800}, 0, nullptr, nullptr);
    pdf_insert_page(ctx, doc, -1, page);
    pdf_drop_obj(ctx, page);
    const QString input = dir.filePath("input.pdf"), output = dir.filePath("signed.pdf");
    pdf_save_document(ctx, doc, QFile::encodeName(input).constData(), &pdf_default_write_options);
    pdf_drop_document(ctx, doc);
    const auto services = openSslSignatureServices();
    PdfDocument pdf(services);
    pdf.open(input);
    if (!pdf.checkDigitalSignatures().isEmpty()) return 9;
    // A failed signing attempt must neither replace the output nor change the open path.
    QFile sentinel(output);
    if (!sentinel.open(QIODevice::WriteOnly) || sentinel.write("unchanged") != 9) return 8;
    sentinel.close();
    try {
        pdf.save(output, {}, {}, {}, {}, pfx, "wrong-password");
        return 3;
    } catch (const std::exception &) {}
    if (!sentinel.open(QIODevice::ReadOnly)) return 8;
    if (sentinel.readAll() != "unchanged" || pdf.path() != input) return 4;
    sentinel.close();
    try {
        pdf.save(output, {{0, {{{50, 80, 250, 50}, "Signed text", 12}}}},
                 {}, {}, {}, pfx, "test-password");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 5;
    }
    if (pdf.path() != output || pdf.fields(0).size() != 1) return 6;
    const auto statuses = pdf.checkDigitalSignatures();
    if (statuses.size() != 1 || !statuses[0].signedField || !statuses[0].digestValid ||
        statuses[0].certificateTrusted || statuses[0].changedSinceSigning ||
        !statuses[0].error.isEmpty() || !statuses[0].signer.contains("jPDF Desk Test")) return 10;

    // Changing a signed byte must fail integrity, even if the PDF still opens.
    const QString tamperedPath = dir.filePath("tampered.pdf");
    if (!QFile::copy(output, tamperedPath)) return 11;
    QFile tampered(tamperedPath);
    if (!tampered.open(QIODevice::ReadWrite) || !tampered.seek(7) || tampered.write("4") != 1) return 11;
    tampered.close();
    PdfDocument tamperedPdf(services);
    tamperedPdf.open(tamperedPath);
    const auto tamperedStatuses = tamperedPdf.checkDigitalSignatures();
    if (tamperedStatuses.size() != 1 || tamperedStatuses[0].digestValid) return 12;

    // Incremental updates preserve signed bytes but must report later revisions.
    const QString revisedPath = dir.filePath("revised.pdf");
    if (!QFile::copy(output, revisedPath)) return 13;
    doc = pdf_open_document(ctx, QFile::encodeName(revisedPath).constData());
    pdf_page *revisedPage = pdf_load_page(ctx, doc, 0);
    pdf_annot *note = pdf_create_annot(ctx, revisedPage, PDF_ANNOT_TEXT);
    pdf_set_annot_rect(ctx, note, {10, 10, 30, 30});
    pdf_set_annot_contents(ctx, note, "Later edit");
    char unsignedName[] = "UnsignedSignature";
    pdf_create_signature_widget(ctx, revisedPage, unsignedName);
    pdf_write_options options = pdf_default_write_options;
    options.do_incremental = 1;
    pdf_save_document(ctx, doc, QFile::encodeName(revisedPath).constData(), &options);
    pdf_drop_page(ctx, revisedPage);
    pdf_drop_document(ctx, doc);
    PdfDocument revisedPdf(services);
    revisedPdf.open(revisedPath);
    const auto revisedStatuses = revisedPdf.checkDigitalSignatures();
    if (revisedStatuses.size() != 2 || !revisedStatuses[0].digestValid ||
        !revisedStatuses[0].changedSinceSigning || revisedStatuses[1].signedField ||
        !revisedStatuses[1].error.isEmpty()) return 14;
    doc = pdf_open_document(ctx, QFile::encodeName(output).constData());
    pdf_page *loaded = pdf_load_page(ctx, doc, 0);
    pdf_annot *widget = pdf_first_widget(ctx, loaded);
    pdf_pkcs7_verifier *verifier = services.verification->createVerifier(ctx);
    const bool valid = widget && pdf_count_signatures(ctx, doc) == 1 &&
        pdf_check_widget_digest(ctx, verifier, widget) == PDF_SIGNATURE_ERROR_OKAY;
    pdf_drop_verifier(ctx, verifier);
    pdf_drop_page(ctx, loaded);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
    return valid ? 0 : 7;
}
