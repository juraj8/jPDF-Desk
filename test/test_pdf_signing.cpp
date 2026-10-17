#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/signing/openssl_provider.h"
#include "jpdf_desk/signing/pdf_signatures.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QCoreApplication>
#include <QBuffer>
#include <QImage>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>

namespace {
// Keep real digest verification while controlling the two chain-policy results.
class ChainPolicyVerifier final : public VerificationProvider {
public:
    std::shared_ptr<const VerificationProvider> delegate;
    pdf_signature_error builtIn = PDF_SIGNATURE_ERROR_UNKNOWN;
    CertificateChainValidation systemChain = CertificateChainValidation::Untrusted;
    QString envelopeError;

    pdf_pkcs7_verifier *createVerifier(fz_context *ctx) const override
    {
        auto *verifier = delegate->createVerifier(ctx);
        verifier->check_certificate = builtIn == PDF_SIGNATURE_ERROR_OKAY
            ? [](fz_context *, pdf_pkcs7_verifier *, unsigned char *, size_t) { return PDF_SIGNATURE_ERROR_OKAY; }
            : [](fz_context *, pdf_pkcs7_verifier *, unsigned char *, size_t) { return PDF_SIGNATURE_ERROR_UNKNOWN; };
        return verifier;
    }
    QString trustDescription() const override { return QStringLiteral("Test chain policy"); }
    SignatureCertificateDetails certificateDetails(const QByteArray &) const override
    {
        SignatureCertificateDetails details;
        details.error = envelopeError;
        details.systemChain = systemChain;
        details.systemChainDescription = QStringLiteral("Injected system-chain diagnostic");
        if (!envelopeError.isEmpty()) {
            details.systemChain = CertificateChainValidation::Accepted;
            details.timestamp.status = SignatureTimestamp::Status::Verified;
            details.timestamp.time = QDateTime::fromSecsSinceEpoch(1);
            details.timestamp.certificateChain = {{"untrusted", "Injected TSA"}};
        }
        return details;
    }
};
}

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
    // One save signs the complete draft, including a newly placed signature image.
    QImage image(30, 10, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) return 27;
    DocumentAnnotations drafts;
    drafts.fields[0] = {{{50, 80, 250, 50}, "Signed text", 12}};
    drafts.signatures[0] = {{{50, 150, 120, 40}, png}};
    try {
        pdf.saveSnapshot(output, drafts, png, {pfx, "test-password"});
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 5;
    }
    if (pdf.path() != output || pdf.fields(0).size() != 1) return 6;
    if (pdf.signatures(0).size() != 1 || pdf.signatures(0).first().png != png
        || pdf.signatureTemplate() != png) return 28;
    const auto statuses = pdf.checkDigitalSignatures();
    if (statuses.size() != 1 || !statuses[0].signedField || !statuses[0].digestValid ||
        statuses[0].certificateTrusted || statuses[0].changedSinceSigning ||
        !statuses[0].error.isEmpty() || !statuses[0].signer.contains("jPDF Desk Test")) return 10;
    const auto &status = statuses[0];
    if (!status.certificateValidFrom.isValid() || !status.certificateValidUntil.isValid()
        || status.certificateValidFrom >= status.certificateValidUntil
        || !status.signingTime.isValid() || !status.certificateProblems.isEmpty()
        || status.certificateStatus != "Self-signed certificate."
        || status.integrity != DigitalSignatureStatus::Integrity::Intact
        || status.certificateValidation != DigitalSignatureStatus::CertificateValidation::Untrusted
        || status.timestamp.status != SignatureTimestamp::Status::Absent) return 15;

    const auto badTime = jpdf::crypto::ValidationTime::atSystemTime(
        jpdf::crypto::detail::validationClockBounds().build);
    const auto clockBlocked = verifySavedPdf(ctx, output, *services.verification, {}, badTime);
    if (clockBlocked.size() != 1 || !clockBlocked[0].digestValid || !clockBlocked[0].error.isEmpty()
        || clockBlocked[0].clockError.isEmpty() || !clockBlocked[0].coverageChecked
        || clockBlocked[0].certificateTrusted
        || clockBlocked[0].certificateValidation != CertificateChainValidation::NotChecked
        || clockBlocked[0].timestamp.status != SignatureTimestamp::Status::NotChecked
        || !clockBlocked[0].signerChain.isEmpty()) return 25;

    // An explicit system rejection must not be lost behind MuPDF's generic error.
    auto chainPolicy = std::make_shared<ChainPolicyVerifier>();
    chainPolicy->delegate = services.verification;
    PdfDocument chainPdf({{}, chainPolicy});
    chainPdf.open(output);
    const auto rejectedChain = chainPdf.checkDigitalSignatures();
    if (rejectedChain.size() != 1 || !rejectedChain[0].error.isEmpty()
        || rejectedChain[0].certificateTrusted
        || rejectedChain[0].certificateValidation != CertificateChainValidation::Untrusted
        || rejectedChain[0].certificateStatus != "Injected system-chain diagnostic") return 22;
    chainPolicy->systemChain = CertificateChainValidation::Failed;
    const auto failedChain = chainPdf.checkDigitalSignatures();
    if (failedChain.size() != 1 || !failedChain[0].error.isEmpty()
        || failedChain[0].certificateValidation != CertificateChainValidation::Failed) return 23;
    // Either provider's acceptance still takes precedence over the other's rejection.
    chainPolicy->systemChain = CertificateChainValidation::Untrusted;
    chainPolicy->builtIn = PDF_SIGNATURE_ERROR_OKAY;
    const auto acceptedChain = chainPdf.checkDigitalSignatures();
    if (acceptedChain.size() != 1 || !acceptedChain[0].error.isEmpty()
        || !acceptedChain[0].certificateTrusted
        || acceptedChain[0].certificateValidation != CertificateChainValidation::Accepted) return 24;

    const auto blockedAcceptance = verifySavedPdf(ctx, output, *chainPolicy, {}, badTime);
    if (blockedAcceptance.size() != 1 || !blockedAcceptance[0].digestValid
        || blockedAcceptance[0].certificateTrusted
        || blockedAcceptance[0].certificateValidation != CertificateChainValidation::NotChecked) return 26;

    chainPolicy->envelopeError = "Unreadable signature envelope";
    const auto unreadable = chainPdf.checkDigitalSignatures();
    if (unreadable.size() != 1 || !unreadable[0].digestValid
        || unreadable[0].error != chainPolicy->envelopeError || unreadable[0].certificateTrusted
        || unreadable[0].certificateValidation != CertificateChainValidation::Failed
        || unreadable[0].timestamp.status != SignatureTimestamp::Status::Failed
        || unreadable[0].timestamp.time.isValid() || !unreadable[0].timestamp.certificateChain.isEmpty()) return 28;

    // Supplemental date diagnostics must identify expiry, not just report unknown trust.
    const QString expiredCert = dir.filePath("expired.pem");
    const QString expiredSignature = dir.filePath("expired.p7s");
    if (!run({"x509", "-in", cert, "-signkey", key, "-days", "0", "-out", expiredCert})
        || !run({"smime", "-sign", "-in", input, "-signer", expiredCert, "-inkey", key,
                 "-outform", "DER", "-out", expiredSignature})) return 16;
    QFile expiredFile(expiredSignature);
    if (!expiredFile.open(QIODevice::ReadOnly)) return 16;
    // A zero-day certificate expires in the generation second.
    QThread::msleep(1100);
    const auto expired = services.verification->certificateDetails(expiredFile.readAll());
    if (!expired.validUntil.isValid() || expired.problems.size() != 1
        || !expired.problems[0].contains("expired")) return 17;
    if (services.verification->certificateDetails("not PKCS7").description.isEmpty()) return 18;

    // Changing a signed byte must fail integrity, even if the PDF still opens.
    const QString tamperedPath = dir.filePath("tampered.pdf");
    if (!QFile::copy(output, tamperedPath)) return 11;
    QFile tampered(tamperedPath);
    if (!tampered.open(QIODevice::ReadWrite) || !tampered.seek(7) || tampered.write("4") != 1) return 11;
    tampered.close();
    PdfDocument tamperedPdf(services);
    tamperedPdf.open(tamperedPath);
    const auto tamperedStatuses = tamperedPdf.checkDigitalSignatures();
    if (tamperedStatuses.size() != 1 || tamperedStatuses[0].digestValid
        || tamperedStatuses[0].integrity != DigitalSignatureStatus::Integrity::Invalid) return 12;

    const auto tamperedClock = verifySavedPdf(ctx, tamperedPath, *services.verification, {}, badTime);
    if (tamperedClock.size() != 1 || tamperedClock[0].clockError.isEmpty()
        || tamperedClock[0].integrity != DigitalSignatureStatus::Integrity::Invalid
        || tamperedClock[0].certificateTrusted) return 27;

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
    // Trust the test root in this process only, never in the host's real CA store.
    const bool hadCaFile = qEnvironmentVariableIsSet("SSL_CERT_FILE");
    const bool hadCaDir = qEnvironmentVariableIsSet("SSL_CERT_DIR");
    const QByteArray previousCaFile = qgetenv("SSL_CERT_FILE"), previousCaDir = qgetenv("SSL_CERT_DIR");
    qputenv("SSL_CERT_FILE", QFile::encodeName(cert));
    qputenv("SSL_CERT_DIR", QFile::encodeName(dir.path()));
    const auto systemTrusted = pdf.checkDigitalSignatures();
    if (systemTrusted.size() != 1 || !systemTrusted[0].certificateTrusted
        || systemTrusted[0].certificateValidation != DigitalSignatureStatus::CertificateValidation::Accepted
        || !systemTrusted[0].certificateDescription.contains("Accepted by system/OpenSSL CA roots")
        || systemTrusted[0].signerChain.size() != 1 || systemTrusted[0].signerChain[0].der.isEmpty()) return 19;
    // A malformed first field must not hide a later real, intact/trusted signature.
    const QString mixedPath = dir.filePath("mixed.pdf");
    if (!QFile::copy(output, mixedPath)) return 29;
    doc = pdf_open_document(ctx, QFile::encodeName(mixedPath).constData());
    pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    pdf_obj *fields = pdf_dict_getp(ctx, root, "AcroForm/Fields");
    pdf_obj *originalValue = pdf_dict_get(ctx, pdf_array_get(ctx, fields, 0), PDF_NAME(V));
    pdf_obj *brokenField = pdf_new_dict(ctx, doc, 3);
    pdf_dict_put(ctx, brokenField, PDF_NAME(FT), PDF_NAME(Sig));
    pdf_dict_put_text_string(ctx, brokenField, PDF_NAME(T), "Malformed PKCS7");
    pdf_obj *brokenValue = pdf_add_new_dict(ctx, doc, 2);
    pdf_dict_put(ctx, brokenField, PDF_NAME(V), brokenValue);
    pdf_dict_put_string(ctx, brokenValue, PDF_NAME(Contents), "not PKCS7", 9);
    pdf_dict_put(ctx, brokenValue, PDF_NAME(ByteRange), pdf_dict_get(ctx, originalValue, PDF_NAME(ByteRange)));
    pdf_drop_obj(ctx, brokenValue);
    pdf_array_insert_drop(ctx, fields, brokenField, 0);
    pdf_save_document(ctx, doc, QFile::encodeName(mixedPath).constData(), &options);
    pdf_drop_document(ctx, doc);
    const auto mixed = verifySavedPdf(ctx, mixedPath, *services.verification);
    if (mixed.size() != 2 || mixed[0].fieldName != "Malformed PKCS7" || mixed[0].error.isEmpty()
        || mixed[0].digestValid || mixed[0].integrity != DigitalSignatureStatus::Integrity::Failed
        || mixed[0].certificateTrusted || mixed[0].certificateValidation != CertificateChainValidation::Failed
        || !mixed[0].signerChain.isEmpty() || !mixed[1].error.isEmpty() || !mixed[1].digestValid
        || !mixed[1].certificateTrusted || !mixed[1].changedSinceSigning) return 30;
    qputenv("SSL_CERT_FILE", QFile::encodeName(expiredCert));
    if (!expiredFile.seek(0)) return 20;
    const auto expiredTrusted = services.verification->certificateDetails(expiredFile.readAll());
    if (expiredTrusted.systemChain != CertificateChainValidation::Failed
        || !expiredTrusted.systemChainDescription.contains("expired")) return 21;
    if (hadCaFile) qputenv("SSL_CERT_FILE", previousCaFile); else qunsetenv("SSL_CERT_FILE");
    if (hadCaDir) qputenv("SSL_CERT_DIR", previousCaDir); else qunsetenv("SSL_CERT_DIR");
    fz_drop_context(ctx);
    return valid ? 0 : 7;
}
