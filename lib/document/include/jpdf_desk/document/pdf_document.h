#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include "jpdf_desk/signing/signature_services.h"
#include <QImage>
#include <QMap>
#include <stdexcept>

// Opening failed because the document requires a valid password.
class PdfPasswordRequired : public std::runtime_error {
public:
    PdfPasswordRequired() : std::runtime_error("This PDF requires a valid password.") {}
};

struct fz_context;
struct pdf_document;

class PdfDocument {
public:
    // Without providers, ordinary PDF operations work without any crypto backend.
    explicit PdfDocument(SignatureServices services = {});
    ~PdfDocument();
    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    // A failed open (including authentication) leaves the current document intact.
    void open(const QString &path, const QString &password = {});
    // Identity interpretation belongs to the injected provider, not the PDF library.
    void saveSnapshot(const QString &path, const DocumentAnnotations &annotations,
              const QByteArray &signatureTemplate = {}, const SigningIdentity &identity = {});
    void save(const QString &path, const QMap<int, QVector<TextField>> &pages,
              const QMap<int, QVector<OptionMark>> &marks = {},
              const QMap<int, QVector<Signature>> &signatures = {},
              const QByteArray &signatureTemplate = {},
              const QString &certificatePath = {}, const QString &certificatePassword = {});
    // Standard PDF Info properties (keys without the "info:" prefix).
    QMap<QString, QString> metadata() const;
    // Updates editable properties; dates and application-private metadata are preserved.
    void setMetadata(const QMap<QString, QString> &values);
    // Applies to the next saved copy; an empty password removes encryption.
    void setPassword(const QString &password);
    QVector<OutlineEntry> outline() const;
    // Case-insensitive selectable-text search. No OCR or unsaved annotation edits.
    QVector<TextSearchMatch> search(const QString &query) const;
    QSizeF pageSize(int pageNumber) const;
    QImage render(int pageNumber, qreal resolution = 1.0) const;
    QImage renderForPrint(int pageNumber, int dpi = 300) const;
    QVector<TextField> fields(int pageNumber) const;
    QVector<OptionMark> marks(int pageNumber) const;
    QVector<Signature> signatures(int pageNumber) const;
    QByteArray signatureTemplate() const;
    // Verify the saved file, independently of edits in the in-memory document.
    QVector<DigitalSignatureStatus> checkDigitalSignatures() const;
    bool canDigitallySign() const { return bool(signatureServices_.signing); }
    bool canVerifySignatures() const { return bool(signatureServices_.verification); }
    QString signatureTrustDescription() const;
    int pageCount() const;
    QString path() const { return path_; }

private:
    fz_context *ctx_ = nullptr;
    pdf_document *doc_ = nullptr;
    QString path_;
    // Kept only in memory to reopen encrypted snapshots and verify signatures.
    QString password_;
    QString savePassword_;
    bool passwordChanged_ = false;
    SignatureServices signatureServices_;
};
