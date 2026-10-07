#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/signing/pdf_signatures.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <utility>
#include <exception>
#include <stdexcept>
#include <cstring>

namespace {
constexpr float scale = 1.5f;
constexpr const char *owner = "jPDF Desk";
constexpr const char *alignedSubject = "jPDF Desk aligned text";
constexpr const char *checkSubject = "jPDF Desk checkmark";
constexpr const char *crossSubject = "jPDF Desk cross";
constexpr const char *signatureSubject = "jPDF Desk signature";
constexpr const char *signatureMetadata = "info:JPDFDeskSignature";
// Read legacy identifiers only for compatibility with already saved PDFs.
constexpr const char *legacyOwner = "PDF Filler";
constexpr const char *legacyAlignedSubject = "PDF Filler aligned text";
constexpr const char *legacyCheckSubject = "PDF Filler checkmark";
constexpr const char *legacyCrossSubject = "PDF Filler cross";
constexpr const char *legacySignatureSubject = "PDF Filler signature";
constexpr const char *legacySignatureMetadata = "info:PdfFillerSignature";

bool matches(const char *value, const char *current, const char *legacy)
{
    return value && (strcmp(value, current) == 0 || strcmp(value, legacy) == 0);
}

bool isOwned(const char *author) { return matches(author, owner, legacyOwner); }
constexpr const char *editableMetadata[] = {"Title", "Author", "Subject", "Keywords", "Creator", "Producer"};
constexpr float markSize = 24.0f; // Scene pixels; keep in sync with MarkItem.

void check(fz_context *ctx, bool failed)
{
    if (failed)
        throw std::runtime_error(fz_caught_message(ctx));
}

QVector<OutlineEntry> outlineEntries(fz_context *ctx, pdf_document *doc,
                                     const fz_outline *node, int depth = 0)
{
    QVector<OutlineEntry> entries;
    if (depth > 100)
        fz_throw(ctx, FZ_ERROR_LIMIT, "PDF outline is too deeply nested");
    for (; node; node = node->next) {
        OutlineEntry entry;
        entry.title = QString::fromUtf8(node->title ? node->title : "");
        if (node->page.page >= 0)
            entry.page = fz_page_number_from_location(ctx, reinterpret_cast<fz_document *>(doc), node->page);
        entry.expanded = node->is_open;
        entry.children = outlineEntries(ctx, doc, node->down, depth + 1);
        entries.append(entry);
    }
    return entries;
}

QRectF toQt(fz_rect r)
{
    return QRectF(r.x0 * scale, r.y0 * scale, (r.x1 - r.x0) * scale,
                  (r.y1 - r.y0) * scale);
}

struct SearchCollector {
    QVector<TextSearchMatch> matches;
    int page = 0;
    std::exception_ptr error;
};

int collectSearchHit(fz_context *, void *opaque, int count, fz_quad *quads)
{
    auto &collector = *static_cast<SearchCollector *>(opaque);
    // Never unwind C++ exceptions through MuPDF's C search callbacks.
    try {
        TextSearchMatch match;
        match.page = collector.page;
        for (int i = 0; i < count; ++i)
            match.rects.append(toQt(fz_rect_from_quad(quads[i])));
        collector.matches.append(std::move(match));
        return 0;
    } catch (...) {
        collector.error = std::current_exception();
        return 1;
    }
}

fz_rect toPdf(QRectF r)
{
    return {float(r.left() / scale), float(r.top() / scale),
            float(r.right() / scale), float(r.bottom() / scale)};
}

// Qt's text layout includes a 4px document margin and has a different
// ascent from MuPDF's Helvetica FreeText appearance (baseline 0.8 * size).
// Apply the offset only to our annotations, and undo it when opening them.
// Units are scene pixels (the PDF page is shown at 1.5x).
qreal textBaselineOffset(float fontSize)
{
    return 4.0 + fontSize * 0.125;
}
}

PdfDocument::PdfDocument(SignatureServices services) : signatureServices_(std::move(services))
{
    ctx_ = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx_)
        throw std::runtime_error("Cannot initialize MuPDF");
    fz_register_document_handlers(ctx_);
}

PdfDocument::~PdfDocument()
{
    if (doc_)
        pdf_drop_document(ctx_, doc_);
    fz_drop_context(ctx_);
}

void PdfDocument::open(const QString &path, const QString &password)
{
    pdf_document *next = nullptr;
    const QByteArray name = QFile::encodeName(path);
    QByteArray secret = password.toUtf8();
    int failed = 0, passwordRequired = 0;
    fz_var(next);
    fz_var(passwordRequired);
    fz_try(ctx_) {
        next = pdf_open_document(ctx_, name.constData());
        if (pdf_needs_password(ctx_, next))
            passwordRequired = !pdf_authenticate_password(ctx_, next, secret.constData());
    }
    fz_catch(ctx_) { failed = 1; }
    secret.fill('\0');
    if (failed || passwordRequired) {
        if (next) pdf_drop_document(ctx_, next);
        check(ctx_, failed);
        throw PdfPasswordRequired();
    }
    if (doc_)
        pdf_drop_document(ctx_, doc_);
    doc_ = next;
    path_ = path;
    password_ = password;
    savePassword_.clear();
    passwordChanged_ = false;
}

void PdfDocument::setPassword(const QString &password)
{
    if (!doc_) throw std::runtime_error("No PDF is open.");
    if (password.contains(QChar(0)) || password.toUtf8().size() > 127)
        throw std::runtime_error("PDF passwords must be at most 127 UTF-8 bytes and cannot contain null characters.");
    savePassword_ = password;
    passwordChanged_ = true;
}

QMap<QString, QString> PdfDocument::metadata() const
{
    QMap<QString, QString> values;
    if (!doc_) return values;
    const auto lookup = [this, &values](const char *key) {
        const QByteArray name = QByteArray("info:") + key;
        QByteArray buffer;
        int failed = 0;
        fz_try(ctx_) {
            const int size = fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                                               name.constData(), nullptr, 0);
            if (size > 0) {
                buffer.resize(size);
                fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                                   name.constData(), buffer.data(), buffer.size());
            }
        }
        fz_catch(ctx_) { failed = 1; }
        check(ctx_, failed);
        values.insert(QString::fromLatin1(key), QString::fromUtf8(buffer.constData()));
    };
    for (const char *key : editableMetadata) lookup(key);
    lookup("CreationDate");
    lookup("ModDate");
    return values;
}

void PdfDocument::setMetadata(const QMap<QString, QString> &values)
{
    if (!doc_) throw std::runtime_error("No PDF is open.");
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        bool supported = false;
        for (const char *key : editableMetadata)
            if (it.key() == QString::fromLatin1(key)) supported = true;
        if (!supported || it.value().contains(QChar(0)))
            throw std::runtime_error("Invalid PDF metadata property or value.");
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QByteArray name = QByteArray("info:") + it.key().toLatin1();
        const QByteArray value = it.value().toUtf8();
        int failed = 0;
        fz_try(ctx_) {
            fz_set_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                            name.constData(), value.constData());
        }
        fz_catch(ctx_) { failed = 1; }
        check(ctx_, failed);
    }
}

QVector<DigitalSignatureStatus> PdfDocument::checkDigitalSignatures() const
{
    if (!doc_) return {};
    if (!signatureServices_.verification)
        throw std::runtime_error("No signature verification provider configured.");
    return verifySavedPdf(ctx_, path_, *signatureServices_.verification, password_);
}

QString PdfDocument::signatureTrustDescription() const
{
    return signatureServices_.verification ? signatureServices_.verification->trustDescription() : QString{};
}

int PdfDocument::pageCount() const
{
    if (!doc_)
        return 0;
    int count = 0, failed = 0;
    fz_try(ctx_) { count = pdf_count_pages(ctx_, doc_); }
    fz_catch(ctx_) { failed = 1; }
    check(ctx_, failed);
    return count;
}

QVector<TextSearchMatch> PdfDocument::search(const QString &query) const
{
    if (!doc_ || query.trimmed().isEmpty()) return {};
    if (query.contains(QChar(0))) throw std::runtime_error("Invalid search text.");
    const QByteArray needle = query.toUtf8();
    SearchCollector collector;
    const int count = pageCount();
    for (int page = 0; page < count; ++page) {
        collector.page = page;
        fz_stext_page *text = nullptr;
        int failed = 0;
        fz_var(text);
        fz_try(ctx_) {
            text = fz_new_stext_page_from_page_number(ctx_, reinterpret_cast<fz_document *>(doc_), page, nullptr);
            fz_search_stext_page_cb(ctx_, text, needle.constData(), collectSearchHit, &collector);
        }
        fz_always(ctx_) { fz_drop_stext_page(ctx_, text); }
        fz_catch(ctx_) { failed = 1; }
        check(ctx_, failed);
        if (collector.error) std::rethrow_exception(collector.error);
    }
    return collector.matches;
}

QVector<OutlineEntry> PdfDocument::outline() const
{
    QVector<OutlineEntry> entries;
    if (!doc_) return entries;
    fz_outline *tree = nullptr;
    int failed = 0;
    fz_var(tree);
    fz_try(ctx_) {
        tree = fz_load_outline(ctx_, reinterpret_cast<fz_document *>(doc_));
        entries = outlineEntries(ctx_, doc_, tree);
    }
    fz_always(ctx_) { fz_drop_outline(ctx_, tree); }
    fz_catch(ctx_) { failed = 1; }
    check(ctx_, failed);
    return entries;
}

QSizeF PdfDocument::pageSize(int pageNumber) const
{
    pdf_page *page = nullptr;
    fz_irect bounds = {};
    int failed = 0;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        bounds = fz_round_rect(fz_transform_rect(
            fz_bound_page(ctx_, reinterpret_cast<fz_page *>(page)), fz_scale(scale, scale)));
    }
    fz_catch(ctx_) { failed = 1; }
    if (page) pdf_drop_page(ctx_, page);
    check(ctx_, failed);
    return QSizeF(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
}

QImage PdfDocument::render(int pageNumber, qreal resolution) const
{
    pdf_page *page = nullptr;
    fz_pixmap *pix = nullptr;
    int failed = 0;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        pix = fz_new_pixmap_from_page_contents(ctx_, reinterpret_cast<fz_page *>(page),
                                                 fz_scale(scale * resolution, scale * resolution), fz_device_rgb(ctx_), 0);
    }
    fz_catch(ctx_) { failed = 1; }
    if (failed) {
        if (page) pdf_drop_page(ctx_, page);
        check(ctx_, true);
    }
    // Copy the pixels before releasing MuPDF's buffer.
    QImage image(fz_pixmap_samples(ctx_, pix), fz_pixmap_width(ctx_, pix),
                 fz_pixmap_height(ctx_, pix), fz_pixmap_stride(ctx_, pix),
                 QImage::Format_RGB888);
    QImage copy = image.copy();
    fz_drop_pixmap(ctx_, pix);
    pdf_drop_page(ctx_, page);
    return copy;
}

QImage PdfDocument::renderForPrint(int pageNumber, int dpi) const
{
    if (!doc_ || dpi <= 0)
        throw std::runtime_error("Invalid document or print resolution.");
    pdf_page *page = nullptr;
    fz_pixmap *pix = nullptr;
    int failed = 0;
    fz_var(page);
    fz_var(pix);
    QImage image;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        pix = pdf_new_pixmap_from_page_with_usage(ctx_, page,
            fz_scale(dpi / 72.0f, dpi / 72.0f), fz_device_rgb(ctx_), 0, "Print", FZ_CROP_BOX);
        image = QImage(fz_pixmap_samples(ctx_, pix), fz_pixmap_width(ctx_, pix),
                       fz_pixmap_height(ctx_, pix), fz_pixmap_stride(ctx_, pix),
                       QImage::Format_RGB888).copy();
    }
    fz_always(ctx_) {
        fz_drop_pixmap(ctx_, pix);
        pdf_drop_page(ctx_, page);
    }
    fz_catch(ctx_) { failed = 1; }
    check(ctx_, failed);
    if (image.isNull()) throw std::runtime_error("Cannot render PDF page for printing.");
    return image;
}

QVector<TextField> PdfDocument::fields(int pageNumber) const
{
    QVector<TextField> result;
    pdf_page *page = nullptr;
    int failed = 0;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;
             annot = pdf_next_annot(ctx_, annot)) {
            const char *author = pdf_annot_author(ctx_, annot);
            if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_FREE_TEXT && isOwned(author)) {
                const char *text = pdf_annot_contents(ctx_, annot);
                const char *font = nullptr;
                float fontSize = 12;
                int components = 0;
                float color[4] = {};
                pdf_annot_default_appearance(ctx_, annot, &font, &fontSize, &components, color);
                if (fontSize <= 0) fontSize = 12;
                QRectF rect = toQt(pdf_annot_rect(ctx_, annot));
                const char *subject = pdf_annot_subject(ctx_, annot);
                if (matches(subject, alignedSubject, legacyAlignedSubject))
                    rect.translate(0, -textBaselineOffset(fontSize));
                result.append({rect, QString::fromUtf8(text ? text : ""), fontSize});
            }
        }
    }
    fz_catch(ctx_) { failed = 1; }
    if (page) pdf_drop_page(ctx_, page);
    check(ctx_, failed);
    return result;
}

QVector<OptionMark> PdfDocument::marks(int pageNumber) const
{
    QVector<OptionMark> result;
    pdf_page *page = nullptr;
    int failed = 0;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;
             annot = pdf_next_annot(ctx_, annot)) {
            if (pdf_annot_type(ctx_, annot) != PDF_ANNOT_INK) continue;
            const char *author = pdf_annot_author(ctx_, annot);
            const char *subject = pdf_annot_subject(ctx_, annot);
            if (!isOwned(author) || !subject) continue;
            const bool isCheck = matches(subject, checkSubject, legacyCheckSubject);
            if ((isCheck || matches(subject, crossSubject, legacyCrossSubject)) &&
                pdf_annot_ink_list_count(ctx_, annot) > 0 &&
                pdf_annot_ink_list_stroke_count(ctx_, annot, 0) > 0) {
                const fz_point first = pdf_annot_ink_list_stroke_vertex(ctx_, annot, 0, 0);
                const float r = markSize / (2 * scale);
                result.append({isCheck ? OptionMark::Check : OptionMark::Cross,
                               QPointF((first.x + (isCheck ? .8f : .7f) * r) * scale,
                                       (first.y + (isCheck ? 0 : .7f) * r) * scale)});
            }
        }
    }
    fz_catch(ctx_) { failed = 1; }
    if (page) pdf_drop_page(ctx_, page);
    check(ctx_, failed);
    return result;
}

QByteArray PdfDocument::signatureTemplate() const
{
    if (!doc_) return {};
    int size = -1, failed = 0;
    const char *key = signatureMetadata;
    fz_var(key);
    fz_try(ctx_) {
        size = fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_), key, nullptr, 0);
        if (size <= 1) {
            key = legacySignatureMetadata;
            size = fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_), key, nullptr, 0);
        }
    }
    fz_catch(ctx_) { failed = 1; }
    check(ctx_, failed);
    if (size <= 1 || size > 8 * 1024 * 1024) return {};
    QByteArray encoded(size, '\0');
    fz_try(ctx_) {
        fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_), key,
                           encoded.data(), encoded.size());
    }
    fz_catch(ctx_) { failed = 1; }
    check(ctx_, failed);
    const QByteArray png = QByteArray::fromBase64(encoded);
    return png.startsWith("\x89PNG\r\n\x1a\n") ? png : QByteArray{};
}

QVector<Signature> PdfDocument::signatures(int pageNumber) const
{
    QVector<Signature> result;
    pdf_page *page = nullptr;
    int failed = 0;
    fz_try(ctx_) {
        page = pdf_load_page(ctx_, doc_, pageNumber);
        for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;
             annot = pdf_next_annot(ctx_, annot)) {
            if (pdf_annot_type(ctx_, annot) != PDF_ANNOT_STAMP) continue;
            const char *author = pdf_annot_author(ctx_, annot);
            const char *subject = pdf_annot_subject(ctx_, annot);
            if (isOwned(author) && matches(subject, signatureSubject, legacySignatureSubject)) {
                const char *contents = pdf_annot_contents(ctx_, annot);
                const QByteArray png = QByteArray::fromBase64(contents ? contents : "");
                if (png.startsWith("\x89PNG\r\n\x1a\n") && png.size() < 6 * 1024 * 1024)
                    result.append({toQt(pdf_annot_rect(ctx_, annot)), png});
            }
        }
    }
    fz_catch(ctx_) { failed = 1; }
    if (page) pdf_drop_page(ctx_, page);
    check(ctx_, failed);
    return result;
}

void PdfDocument::saveSnapshot(const QString &path, const DocumentAnnotations &annotations,
                               const QByteArray &signatureTemplate, const SigningIdentity &identity)
{
    save(path, annotations.fields, annotations.marks, annotations.signatures,
         signatureTemplate, identity.identity, identity.password);
}

void PdfDocument::save(const QString &path, const QMap<int, QVector<TextField>> &pages,
                       const QMap<int, QVector<OptionMark>> &marks,
                       const QMap<int, QVector<Signature>> &signatures,
                       const QByteArray &signatureTemplate,
                       const QString &certificatePath, const QString &certificatePassword)
{
    // Save to a separate file: never overwrite the source while MuPDF has it open.
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(path_).absoluteFilePath())
        throw std::runtime_error("Choose a different output file (the source PDF is open).");
    if (!certificatePath.isEmpty() && !signatureServices_.signing)
        throw std::runtime_error("No signing provider configured.");
    QTemporaryDir staging;
    if (!certificatePath.isEmpty() && !staging.isValid())
        throw std::runtime_error("Cannot create temporary signing directory.");
    const QString savePath = certificatePath.isEmpty() ? path : staging.filePath("filled.pdf");
    const QString outputPassword = passwordChanged_ ? savePassword_ : password_;
    pdf_write_options options = pdf_default_write_options;
    if (passwordChanged_) {
        options.do_encrypt = outputPassword.isEmpty() ? PDF_ENCRYPT_NONE : PDF_ENCRYPT_AES_256;
        const QByteArray secret = outputPassword.toUtf8();
        // Use the new password for both opening and owner authentication.
        std::memcpy(options.upwd_utf8, secret.constData(), secret.size() + 1);
        std::memcpy(options.opwd_utf8, secret.constData(), secret.size() + 1);
    }
    pdf_page *page = nullptr;
    int failed = 0;
    fz_try(ctx_) {
        for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
        page = pdf_load_page(ctx_, doc_, it.key());
        for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;) {
            pdf_annot *next = pdf_next_annot(ctx_, annot);
            const char *author = pdf_annot_author(ctx_, annot);
            if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_FREE_TEXT && isOwned(author))
                pdf_delete_annot(ctx_, page, annot);
            annot = next;
        }
        for (const TextField &field : it.value()) {
            if (field.text.trimmed().isEmpty()) continue;
            pdf_annot *annot = pdf_create_annot(ctx_, page, PDF_ANNOT_FREE_TEXT);
            pdf_set_annot_rect(ctx_, annot,
                toPdf(field.rect.translated(0, textBaselineOffset(field.fontSize))));
            pdf_set_annot_contents(ctx_, annot, field.text.toUtf8().constData());
            pdf_set_annot_author(ctx_, annot, owner);
            pdf_set_annot_subject(ctx_, annot, alignedSubject);
            const float black[] = {0, 0, 0};
            pdf_set_annot_default_appearance(ctx_, annot, "Helv", field.fontSize, 3, black);
            pdf_annot_request_synthesis(ctx_, annot);
        }
        pdf_update_page(ctx_, page);
        pdf_drop_page(ctx_, page);
        page = nullptr;
        }
        for (auto it = marks.cbegin(); it != marks.cend(); ++it) {
            page = pdf_load_page(ctx_, doc_, it.key());
            for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;) {
                pdf_annot *next = pdf_next_annot(ctx_, annot);
                const char *author = pdf_annot_author(ctx_, annot);
                const char *subject = pdf_annot_subject(ctx_, annot);
                if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_INK && isOwned(author) &&
                    (matches(subject, checkSubject, legacyCheckSubject) ||
                     matches(subject, crossSubject, legacyCrossSubject)))
                    pdf_delete_annot(ctx_, page, annot);
                annot = next;
            }
            for (const OptionMark &mark : it.value()) {
                pdf_annot *annot = pdf_create_annot(ctx_, page, PDF_ANNOT_INK);
                pdf_set_annot_author(ctx_, annot, owner);
                pdf_set_annot_subject(ctx_, annot, mark.kind == OptionMark::Check ? checkSubject : crossSubject);
                pdf_set_annot_border_width(ctx_, annot, 2.0f / scale);
                const float black[] = {0, 0, 0};
                pdf_set_annot_color(ctx_, annot, 3, black);
                const float x = float(mark.center.x() / scale);
                const float y = float(mark.center.y() / scale);
                const float r = markSize / (2 * scale);
                if (mark.kind == OptionMark::Check) {
                    fz_point stroke[] = {{x - r * .8f, y}, {x - r * .2f, y + r * .6f},
                                         {x + r * .9f, y - r * .7f}};
                    pdf_add_annot_ink_list(ctx_, annot, 3, stroke);
                } else {
                    fz_point first[] = {{x - r * .7f, y - r * .7f}, {x + r * .7f, y + r * .7f}};
                    fz_point second[] = {{x + r * .7f, y - r * .7f}, {x - r * .7f, y + r * .7f}};
                    pdf_add_annot_ink_list(ctx_, annot, 2, first);
                    pdf_add_annot_ink_list(ctx_, annot, 2, second);
                }
            }
            pdf_update_page(ctx_, page);
            pdf_drop_page(ctx_, page);
            page = nullptr;
        }
        for (auto it = signatures.cbegin(); it != signatures.cend(); ++it) {
            page = pdf_load_page(ctx_, doc_, it.key());
            for (pdf_annot *annot = pdf_first_annot(ctx_, page); annot;) {
                pdf_annot *next = pdf_next_annot(ctx_, annot);
                const char *author = pdf_annot_author(ctx_, annot);
                const char *subject = pdf_annot_subject(ctx_, annot);
                if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_STAMP && isOwned(author) &&
                    matches(subject, signatureSubject, legacySignatureSubject))
                    pdf_delete_annot(ctx_, page, annot);
                annot = next;
            }
            for (const Signature &signature : it.value()) {
                if (signature.png.isEmpty()) continue;
                const QByteArray encoded = signature.png.toBase64();
                fz_buffer *buffer = nullptr;
                fz_image *image = nullptr;
                fz_try(ctx_) {
                    buffer = fz_new_buffer_from_copied_data(ctx_,
                        reinterpret_cast<const unsigned char *>(signature.png.constData()),
                        signature.png.size());
                    image = fz_new_image_from_buffer(ctx_, buffer);
                    pdf_annot *annot = pdf_create_annot(ctx_, page, PDF_ANNOT_STAMP);
                    pdf_set_annot_rect(ctx_, annot, toPdf(signature.rect));
                    pdf_set_annot_author(ctx_, annot, owner);
                    pdf_set_annot_subject(ctx_, annot, signatureSubject);
                    pdf_set_annot_contents(ctx_, annot, encoded.constData());
                    pdf_set_annot_stamp_image(ctx_, annot, image);
                }
                fz_always(ctx_) {
                    fz_drop_image(ctx_, image);
                    fz_drop_buffer(ctx_, buffer);
                }
                fz_catch(ctx_) { fz_rethrow(ctx_); }
            }
            pdf_update_page(ctx_, page);
            pdf_drop_page(ctx_, page);
            page = nullptr;
        }
        if (!signatureTemplate.isEmpty()) {
            const QByteArray encoded = signatureTemplate.toBase64();
            fz_set_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                            signatureMetadata, encoded.constData());
        }
        pdf_save_document(ctx_, doc_, QFile::encodeName(savePath).constData(), &options);
    }
    fz_catch(ctx_) { failed = 1; }
    if (page) pdf_drop_page(ctx_, page);
    check(ctx_, failed);
    if (!certificatePath.isEmpty()) {
        signPdfSnapshot(ctx_, savePath, path, *signatureServices_.signing,
                        {certificatePath, certificatePassword}, outputPassword);
    }
    // Reopen the output: later edits operate on the saved annotations, not a stale source.
    open(path, outputPassword);
}
