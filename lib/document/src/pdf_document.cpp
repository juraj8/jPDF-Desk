#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/document/detail/mupdf_call.h"
#include "jpdf_desk/signing/pdf_signatures.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QSet>
#include <utility>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <cmath>

namespace {
constexpr float scale = 1.5f;
constexpr const char *owner = "jPDF Desk";
constexpr const char *alignedSubject = "jPDF Desk aligned text";
constexpr const char *checkSubject = "jPDF Desk checkmark";
constexpr const char *crossSubject = "jPDF Desk cross";
constexpr const char *signatureIdKey = "JPDFDeskSignatureId";
constexpr const char *signatureRectKey = "JPDFDeskSignatureRect";
constexpr const char *signatureMetadata = "info:JPDFDeskSignature";
bool matches(const char *value, const char *expected)
{
    return value && strcmp(value, expected) == 0;
}

bool isOwned(const char *author) { return matches(author, owner); }
constexpr const char *editableMetadata[] = {"Title", "Author", "Subject", "Keywords", "Creator", "Producer"};
constexpr float markSize = 24.0f; // Scene pixels; keep in sync with MarkItem.

QVector<OutlineEntry> outlineEntries(fz_context *ctx, pdf_document *doc,
                                     const fz_outline *node, int depth = 0)
{
    QVector<OutlineEntry> entries;
    if (depth > 100)
        throw std::runtime_error("PDF outline is too deeply nested");
    for (; node; node = node->next) {
        OutlineEntry entry;
        entry.title = QString::fromUtf8(node->title ? node->title : "");
        if (node->page.page >= 0)
            entry.page = mupdf::call(ctx, [&]() noexcept {
                return fz_page_number_from_location(ctx, reinterpret_cast<fz_document *>(doc), node->page);
            });
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
    try {
        mupdf::call(ctx_, [&]() noexcept { fz_register_document_handlers(ctx_); });
    } catch (...) {
        fz_drop_context(ctx_);
        throw;
    }
}

PdfDocument::~PdfDocument()
{
    if (doc_)
        pdf_drop_document(ctx_, doc_);
    fz_drop_context(ctx_);
}

void PdfDocument::open(const QString &path, const QString &password)
{
    auto next = mupdf::own(ctx_, static_cast<pdf_document *>(nullptr), pdf_drop_document);
    // MuPDF expects UTF-8 paths, including on Windows (not the local code page).
    const QByteArray name = path.toUtf8();
    QByteArray secret = password.toUtf8();
    try {
        next.reset(mupdf::call(ctx_, [&]() noexcept {
            return pdf_open_document(ctx_, name.constData());
        }));
        const bool passwordRequired = mupdf::call(ctx_, [&]() noexcept {
            return pdf_needs_password(ctx_, next.get()) &&
                !pdf_authenticate_password(ctx_, next.get(), secret.constData());
        });
        if (passwordRequired) throw PdfPasswordRequired();
    } catch (...) {
        secret.fill('\0');
        throw;
    }
    secret.fill('\0');
    if (doc_)
        pdf_drop_document(ctx_, doc_);
    doc_ = next.release();
    path_ = path;
    password_ = password;
    savePassword_.clear();
    passwordChanged_ = false;
}

void PdfDocument::swapContent(PdfDocument &other) noexcept
{
    std::swap(ctx_, other.ctx_);
    std::swap(doc_, other.doc_);
    path_.swap(other.path_);
    password_.swap(other.password_);
    savePassword_.swap(other.savePassword_);
    std::swap(passwordChanged_, other.passwordChanged_);
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
        const int size = mupdf::call(ctx_, [&]() noexcept {
            return fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                                      name.constData(), nullptr, 0);
        });
        QByteArray buffer;
        if (size > 0) {
            buffer.resize(size);
            char *data = buffer.data();
            mupdf::call(ctx_, [&]() noexcept {
                fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                                   name.constData(), data, size);
            });
        }
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
        mupdf::call(ctx_, [&]() noexcept {
            fz_set_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                            name.constData(), value.constData());
        });
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
    return mupdf::call(ctx_, [&]() noexcept { return pdf_count_pages(ctx_, doc_); });
}

QVector<TextSearchMatch> PdfDocument::search(const QString &query,
    const std::function<bool(int, int)> &progress) const
{
    if (!doc_ || query.trimmed().isEmpty()) return {};
    if (query.contains(QChar(0))) throw std::runtime_error("Invalid search text.");
    const QByteArray needle = query.toUtf8();
    SearchCollector collector;
    const int count = pageCount();
    for (int page = 0; page < count; ++page) {
        if (progress && !progress(page, count)) throw PdfOperationCancelled();
        collector.page = page;
        auto text = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_new_stext_page_from_page_number(ctx_, reinterpret_cast<fz_document *>(doc_), page, nullptr);
        }), fz_drop_stext_page);
        mupdf::call(ctx_, [&]() noexcept {
            fz_search_stext_page_cb(ctx_, text.get(), needle.constData(), collectSearchHit, &collector);
        });
        if (collector.error) std::rethrow_exception(collector.error);
    }
    if (progress && !progress(count, count)) throw PdfOperationCancelled();
    return collector.matches;
}

QVector<OutlineEntry> PdfDocument::outline() const
{
    if (!doc_) return {};
    auto tree = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return fz_load_outline(ctx_, reinterpret_cast<fz_document *>(doc_));
    }), fz_drop_outline);
    return outlineEntries(ctx_, doc_, tree.get());
}

QVector<PdfPageLink> PdfDocument::links(int pageNumber) const
{
    if (!doc_) return {};
    auto *document = reinterpret_cast<fz_document *>(doc_);
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    auto links = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return fz_load_links(ctx_, reinterpret_cast<fz_page *>(page.get()));
    }), fz_drop_link);
    QVector<PdfPageLink> result;
    for (auto *link = links.get(); link; link = link->next) {
        // Never execute actions or open external files/URLs from page links.
        if (!link->uri || fz_is_external_link(ctx_, link->uri)) continue;
        const auto dest = mupdf::call(ctx_, [&]() noexcept {
            return fz_resolve_link_dest(ctx_, document, link->uri);
        });
        if (dest.loc.page < 0) continue;
        const int target = mupdf::call(ctx_, [&]() noexcept {
            return fz_page_number_from_location(ctx_, document, dest.loc);
        });
        if (target < 0 || target >= pageCount()) continue;
        result.append({toQt(link->rect), target,
                       QPointF(std::isfinite(dest.x) ? dest.x * scale : 0,
                               std::isfinite(dest.y) ? dest.y * scale : 0)});
    }
    return result;
}

QSizeF PdfDocument::pageSize(int pageNumber) const
{
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    const fz_irect bounds = mupdf::call(ctx_, [&]() noexcept {
        return fz_round_rect(fz_transform_rect(
            fz_bound_page(ctx_, reinterpret_cast<fz_page *>(page.get())), fz_scale(scale, scale)));
    });
    return QSizeF(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
}

QImage PdfDocument::render(int pageNumber, qreal resolution) const
{
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    auto pix = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return fz_new_pixmap_from_page_contents(ctx_, reinterpret_cast<fz_page *>(page.get()),
            fz_scale(scale * resolution, scale * resolution), fz_device_rgb(ctx_), 0);
    }), fz_drop_pixmap);
    // Native form widgets are separate from page contents. Application-owned
    // annotations remain scene items and must not be painted twice.
    auto device = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return fz_new_draw_device(ctx_, fz_identity, pix.get());
    }), fz_drop_device);
    mupdf::call(ctx_, [&]() noexcept {
        pdf_update_page(ctx_, page.get());
        pdf_run_page_widgets(ctx_, page.get(), device.get(),
                             fz_scale(scale * resolution, scale * resolution), nullptr);
        fz_close_device(ctx_, device.get());
    });
    // Qt allocations and the pixel copy happen outside the longjmp boundary.
    return QImage(fz_pixmap_samples(ctx_, pix.get()), fz_pixmap_width(ctx_, pix.get()),
                  fz_pixmap_height(ctx_, pix.get()), fz_pixmap_stride(ctx_, pix.get()),
                  QImage::Format_RGB888).copy();
}

QImage PdfDocument::renderForPrint(int pageNumber, int dpi) const
{
    if (!doc_ || dpi <= 0)
        throw std::runtime_error("Invalid document or print resolution.");
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    auto pix = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_new_pixmap_from_page_with_usage(ctx_, page.get(),
            fz_scale(dpi / 72.0f, dpi / 72.0f), fz_device_rgb(ctx_), 0, "Print", FZ_CROP_BOX);
    }), fz_drop_pixmap);
    QImage image = QImage(fz_pixmap_samples(ctx_, pix.get()), fz_pixmap_width(ctx_, pix.get()),
                          fz_pixmap_height(ctx_, pix.get()), fz_pixmap_stride(ctx_, pix.get()),
                          QImage::Format_RGB888).copy();
    if (image.isNull()) throw std::runtime_error("Cannot render PDF page for printing.");
    return image;
}

QVector<TextField> PdfDocument::fields(int pageNumber) const
{
    QVector<TextField> result;
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    for (pdf_annot *annot = pdf_first_annot(ctx_, page.get()); annot;
         annot = pdf_next_annot(ctx_, annot)) {
        const auto field = mupdf::call(ctx_, [&]() noexcept {
            struct FieldData { bool owned; fz_rect rect; const char *text; float size; bool aligned; };
            FieldData data{};
            if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_FREE_TEXT &&
                isOwned(pdf_annot_author(ctx_, annot))) {
                data.owned = true;
                data.text = pdf_annot_contents(ctx_, annot);
                const char *font = nullptr;
                int components = 0;
                float color[4] = {};
                pdf_annot_default_appearance(ctx_, annot, &font, &data.size, &components, color);
                data.rect = pdf_annot_rect(ctx_, annot);
                data.aligned = matches(pdf_annot_subject(ctx_, annot), alignedSubject);
            }
            return data;
        });
        if (!field.owned) continue;
        const float size = field.size > 0 ? field.size : 12;
        QRectF rect = toQt(field.rect);
        if (field.aligned) rect.translate(0, -textBaselineOffset(size));
        result.append({rect, QString::fromUtf8(field.text ? field.text : ""), size});
    }
    return result;
}

QVector<OptionMark> PdfDocument::marks(int pageNumber) const
{
    QVector<OptionMark> result;
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    for (pdf_annot *annot = pdf_first_annot(ctx_, page.get()); annot;
         annot = pdf_next_annot(ctx_, annot)) {
        const auto mark = mupdf::call(ctx_, [&]() noexcept {
            struct MarkData { bool owned; bool check; fz_point first; };
            MarkData data{};
            if (pdf_annot_type(ctx_, annot) != PDF_ANNOT_INK ||
                !isOwned(pdf_annot_author(ctx_, annot))) return data;
            const char *subject = pdf_annot_subject(ctx_, annot);
            data.check = matches(subject, checkSubject);
            if ((data.check || matches(subject, crossSubject)) &&
                pdf_annot_ink_list_count(ctx_, annot) > 0 &&
                pdf_annot_ink_list_stroke_count(ctx_, annot, 0) > 0) {
                data.owned = true;
                data.first = pdf_annot_ink_list_stroke_vertex(ctx_, annot, 0, 0);
            }
            return data;
        });
        if (!mark.owned) continue;
        const float r = markSize / (2 * scale);
        result.append({mark.check ? OptionMark::Check : OptionMark::Cross,
                       QPointF((mark.first.x + (mark.check ? .8f : .7f) * r) * scale,
                               (mark.first.y + (mark.check ? 0 : .7f) * r) * scale)});
    }
    return result;
}

QByteArray PdfDocument::signatureTemplate() const
{
    if (!doc_) return {};
    const int size = mupdf::call(ctx_, [&]() noexcept {
        return fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_), signatureMetadata, nullptr, 0);
    });
    if (size <= 1 || size > 8 * 1024 * 1024) return {};
    QByteArray encoded(size, '\0');
    char *data = encoded.data();
    mupdf::call(ctx_, [&]() noexcept {
        fz_lookup_metadata(ctx_, reinterpret_cast<fz_document *>(doc_), signatureMetadata, data, size);
    });
    const QByteArray png = QByteArray::fromBase64(encoded);
    return png.startsWith("\x89PNG\r\n\x1a\n") ? png : QByteArray{};
}

QVector<Signature> PdfDocument::signatures(int pageNumber) const
{
    QVector<Signature> result;
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    pdf_obj *contents = mupdf::call(ctx_, [&]() noexcept {
        return pdf_dict_get(ctx_, page->obj, PDF_NAME(Contents));
    });
    const int count = mupdf::call(ctx_, [&]() noexcept {
        return pdf_is_array(ctx_, contents) ? pdf_array_len(ctx_, contents) : 1;
    });
    for (int i = 0; i < count; ++i) {
        const auto data = mupdf::call(ctx_, [&]() noexcept {
            pdf_obj *stream = pdf_is_array(ctx_, contents) ? pdf_array_get(ctx_, contents, i) : contents;
            struct SavedSignature { const char *id; fz_rect rect; };
            return SavedSignature{
                pdf_dict_gets(ctx_, stream, signatureRectKey)
                    ? pdf_to_text_string(ctx_, pdf_dict_gets(ctx_, stream, signatureIdKey)) : "",
                pdf_to_rect(ctx_, pdf_dict_gets(ctx_, stream, signatureRectKey))};
        });
        const QString id = QString::fromUtf8(data.id);
        if (!id.isEmpty() && !fz_is_empty_rect(data.rect) && !fz_is_infinite_rect(data.rect))
            result.append({toQt(data.rect), {}, id});
    }
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
    // Never overwrite the source while MuPDF has it open.
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(path_).absoluteFilePath())
        throw std::runtime_error("Choose a different output file (the source PDF is open).");
    if (!certificatePath.isEmpty() && !signatureServices_.signing)
        throw std::runtime_error("No signing provider configured.");
    auto copy = snapshot({pages, marks, signatures}, signatureTemplate);
    QTemporaryDir staging;
    if (!staging.isValid())
        throw std::runtime_error("Cannot create temporary export directory.");
    const QString savePath = staging.filePath("filled.pdf");
    QString publishPath = savePath;
    const QString outputPassword = passwordChanged_ ? savePassword_ : password_;
    pdf_write_options options = pdf_default_write_options;
    if (passwordChanged_) {
        options.do_encrypt = outputPassword.isEmpty() ? PDF_ENCRYPT_NONE : PDF_ENCRYPT_AES_256;
        const QByteArray secret = outputPassword.toUtf8();
        // Use the new password for both opening and owner authentication.
        std::memcpy(options.upwd_utf8, secret.constData(), secret.size() + 1);
        std::memcpy(options.opwd_utf8, secret.constData(), secret.size() + 1);
    }
    const QByteArray saveName = savePath.toUtf8();
    mupdf::call(copy->ctx_, [&]() noexcept {
        pdf_save_document(copy->ctx_, copy->doc_, saveName.constData(), &options);
    });
    if (!certificatePath.isEmpty()) {
        publishPath = staging.filePath("signed.pdf");
        signPdfSnapshot(copy->ctx_, savePath, publishPath, *signatureServices_.signing,
                        {certificatePath, certificatePassword}, outputPassword);
    }
    // Validate and authenticate before publication, so a failed reopen cannot
    // replace the destination or change the live document.
    copy->openBuffered(publishPath, outputPassword);
    QFile source(publishPath);
    QSaveFile destination(path);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot open PDF output.");
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() || destination.write(chunk) != chunk.size())
            throw std::runtime_error("Cannot copy PDF output.");
    }
    // Preallocate the final path before committing; adoption below cannot fail.
    copy->path_ = path;
    if (!destination.commit()) throw std::runtime_error("Cannot commit PDF output.");
    std::swap(ctx_, copy->ctx_);
    std::swap(doc_, copy->doc_);
    path_.swap(copy->path_);
    password_.swap(copy->password_);
    savePassword_.clear();
    passwordChanged_ = false;
}

void PdfDocument::openBuffered(const QString &path, const QString &password)
{
    auto next = mupdf::own(ctx_, static_cast<pdf_document *>(nullptr), pdf_drop_document);
    const QByteArray name = path.toUtf8();
    QByteArray secret = password.toUtf8();
    try {
        auto buffer = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_read_file(ctx_, name.constData());
        }), fz_drop_buffer);
        auto stream = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_open_buffer(ctx_, buffer.get());
        }), fz_drop_stream);
        next.reset(mupdf::call(ctx_, [&]() noexcept {
            return pdf_open_document_with_stream(ctx_, stream.get());
        }));
        mupdf::call(ctx_, [&]() noexcept {
            if (pdf_needs_password(ctx_, next.get()) &&
                !pdf_authenticate_password(ctx_, next.get(), secret.constData()))
                fz_throw(ctx_, FZ_ERROR_ARGUMENT, "Cannot authenticate PDF export.");
        });
    } catch (...) {
        secret.fill('\0');
        throw;
    }
    secret.fill('\0');
    pdf_drop_document(ctx_, doc_);
    doc_ = next.release();
    password_ = password;
}

std::unique_ptr<PdfDocument> PdfDocument::snapshot(const DocumentAnnotations &annotations,
                                                 const QByteArray &signatureTemplate) const
{
    if (!doc_) throw std::runtime_error("No PDF is open.");
    auto copy = std::make_unique<PdfDocument>();
    // Serialize the current document, not its on-disk source, so unsaved
    // metadata is included. Keep encryption and authenticate within this class.
    QByteArray secret = password_.toUtf8();
    try {
        auto buffer = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_new_buffer(ctx_, 0);
        }), fz_drop_buffer);
        auto output = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_new_output_with_buffer(ctx_, buffer.get());
        }), fz_drop_output);
        mupdf::call(ctx_, [&]() noexcept {
            pdf_write_document(ctx_, doc_, output.get(), &pdf_default_write_options);
            fz_close_output(ctx_, output.get());
        });
        auto stream = mupdf::own(copy->ctx_, mupdf::call(copy->ctx_, [&]() noexcept {
            return fz_open_buffer(copy->ctx_, buffer.get());
        }), fz_drop_stream);
        copy->doc_ = mupdf::call(copy->ctx_, [&]() noexcept {
            return pdf_open_document_with_stream(copy->ctx_, stream.get());
        });
        mupdf::call(copy->ctx_, [&]() noexcept {
            if (pdf_needs_password(copy->ctx_, copy->doc_) &&
                !pdf_authenticate_password(copy->ctx_, copy->doc_, secret.constData()))
                fz_throw(copy->ctx_, FZ_ERROR_ARGUMENT, "Cannot authenticate PDF snapshot.");
        });
    } catch (...) {
        secret.fill('\0');
        throw;
    }
    secret.fill('\0');
    copy->password_ = password_;
    copy->applyAnnotations(annotations, signatureTemplate);
    return copy;
}

void PdfDocument::applyAnnotations(const DocumentAnnotations &annotations,
                                   const QByteArray &signatureTemplate)
{
    for (auto it = annotations.fields.cbegin(); it != annotations.fields.cend(); ++it) {
        auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return pdf_load_page(ctx_, doc_, it.key());
        }), pdf_drop_page);
        mupdf::call(ctx_, [&]() noexcept {
            for (pdf_annot *annot = pdf_first_annot(ctx_, page.get()); annot;) {
                pdf_annot *next = pdf_next_annot(ctx_, annot);
                if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_FREE_TEXT &&
                    isOwned(pdf_annot_author(ctx_, annot)))
                    pdf_delete_annot(ctx_, page.get(), annot);
                annot = next;
            }
        });
        for (const TextField &field : it.value()) {
            if (field.text.trimmed().isEmpty()) continue;
            const QByteArray text = field.text.toUtf8();
            const fz_rect rect = toPdf(field.rect.translated(0, textBaselineOffset(field.fontSize)));
            auto annot = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return pdf_create_annot(ctx_, page.get(), PDF_ANNOT_FREE_TEXT);
            }), pdf_drop_annot);
            mupdf::call(ctx_, [&]() noexcept {
                pdf_set_annot_rect(ctx_, annot.get(), rect);
                pdf_set_annot_contents(ctx_, annot.get(), text.constData());
                pdf_set_annot_author(ctx_, annot.get(), owner);
                pdf_set_annot_subject(ctx_, annot.get(), alignedSubject);
                const float black[] = {0, 0, 0};
                pdf_set_annot_default_appearance(ctx_, annot.get(), "Helv", field.fontSize, 3, black);
                pdf_annot_request_synthesis(ctx_, annot.get());
            });
        }
        mupdf::call(ctx_, [&]() noexcept { pdf_update_page(ctx_, page.get()); });
    }
    for (auto it = annotations.marks.cbegin(); it != annotations.marks.cend(); ++it) {
        auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return pdf_load_page(ctx_, doc_, it.key());
        }), pdf_drop_page);
        mupdf::call(ctx_, [&]() noexcept {
            for (pdf_annot *annot = pdf_first_annot(ctx_, page.get()); annot;) {
                pdf_annot *next = pdf_next_annot(ctx_, annot);
                const char *author = pdf_annot_author(ctx_, annot);
                const char *subject = pdf_annot_subject(ctx_, annot);
                if (pdf_annot_type(ctx_, annot) == PDF_ANNOT_INK && isOwned(author) &&
                    (matches(subject, checkSubject) || matches(subject, crossSubject)))
                    pdf_delete_annot(ctx_, page.get(), annot);
                annot = next;
            }
        });
        for (const OptionMark &mark : it.value()) {
            auto annot = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return pdf_create_annot(ctx_, page.get(), PDF_ANNOT_INK);
            }), pdf_drop_annot);
            const float x = float(mark.center.x() / scale);
            const float y = float(mark.center.y() / scale);
            const float r = markSize / (2 * scale);
            mupdf::call(ctx_, [&]() noexcept {
                pdf_set_annot_author(ctx_, annot.get(), owner);
                pdf_set_annot_subject(ctx_, annot.get(), mark.kind == OptionMark::Check ? checkSubject : crossSubject);
                pdf_set_annot_border_width(ctx_, annot.get(), 2.0f / scale);
                const float black[] = {0, 0, 0};
                pdf_set_annot_color(ctx_, annot.get(), 3, black);
                if (mark.kind == OptionMark::Check) {
                    fz_point stroke[] = {{x - r * .8f, y}, {x - r * .2f, y + r * .6f},
                                         {x + r * .9f, y - r * .7f}};
                    pdf_add_annot_ink_list(ctx_, annot.get(), 3, stroke);
                } else {
                    fz_point first[] = {{x - r * .7f, y - r * .7f}, {x + r * .7f, y + r * .7f}};
                    fz_point second[] = {{x + r * .7f, y - r * .7f}, {x - r * .7f, y + r * .7f}};
                    pdf_add_annot_ink_list(ctx_, annot.get(), 2, first);
                    pdf_add_annot_ink_list(ctx_, annot.get(), 2, second);
                }
            });
        }
        mupdf::call(ctx_, [&]() noexcept { pdf_update_page(ctx_, page.get()); });
    }
    for (auto it = annotations.signatures.cbegin(); it != annotations.signatures.cend(); ++it) {
        auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return pdf_load_page(ctx_, doc_, it.key());
        }), pdf_drop_page);
        QSet<QString> retained;
        bool hasNewImages = false;
        for (const auto &signature : it.value()) {
            if (!signature.id.isEmpty()) retained.insert(signature.id);
            else if (!signature.png.isEmpty()) hasNewImages = true;
        }
        auto streams = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return pdf_new_array(ctx_, doc_, 4);
        }), pdf_drop_obj);
        pdf_obj *original = mupdf::call(ctx_, [&]() noexcept {
            return pdf_dict_get(ctx_, page->obj, PDF_NAME(Contents));
        });
        const int count = mupdf::call(ctx_, [&]() noexcept {
            return pdf_is_array(ctx_, original) ? pdf_array_len(ctx_, original) : 1;
        });
        for (int i = 0; i < count; ++i) {
            pdf_obj *stream = mupdf::call(ctx_, [&]() noexcept {
                return pdf_is_array(ctx_, original) ? pdf_array_get(ctx_, original, i) : original;
            });
            const char *id = mupdf::call(ctx_, [&]() noexcept {
                return pdf_to_text_string(ctx_, pdf_dict_gets(ctx_, stream, signatureIdKey));
            });
            if (stream && (!id || !*id || retained.contains(QString::fromUtf8(id))))
                mupdf::call(ctx_, [&]() noexcept { pdf_array_push(ctx_, streams.get(), stream); });
        }
        mupdf::call(ctx_, [&]() noexcept { pdf_dict_put(ctx_, page->obj, PDF_NAME(Contents), streams.get()); });
        if (!hasNewImages) continue;
        // Work in page coordinates (including CropBox/Rotate/UserUnit), with
        // page-local resources so inherited resources are not replaced.
        auto resources = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            pdf_obj *existing = pdf_page_resources(ctx_, page.get());
            return existing ? pdf_deep_copy_obj(ctx_, existing) : pdf_new_dict(ctx_, doc_, 4);
        }), pdf_drop_obj);
        // Isolate the original graphics state once. Each signature then has its
        // own balanced stream, so removing one cannot affect other content.
        auto prefix = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_new_buffer_from_copied_data(ctx_, reinterpret_cast<const unsigned char *>("q\n"), 2);
        }), fz_drop_buffer);
        auto suffix = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return fz_new_buffer_from_copied_data(ctx_, reinterpret_cast<const unsigned char *>("Q\n"), 2);
        }), fz_drop_buffer);
        mupdf::call(ctx_, [&]() noexcept {
            pdf_array_insert_drop(ctx_, streams.get(), pdf_add_stream(ctx_, doc_, prefix.get(), nullptr, 0), 0);
            pdf_array_push_drop(ctx_, streams.get(), pdf_add_stream(ctx_, doc_, suffix.get(), nullptr, 0));
        });
        for (const Signature &signature : it.value()) {
            if (!signature.id.isEmpty() || signature.png.isEmpty()) continue;
            auto contents = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return fz_new_buffer(ctx_, 256);
            }), fz_drop_buffer);
            mupdf::call(ctx_, [&]() noexcept { fz_append_string(ctx_, contents.get(), "q\n"); });
            auto device = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                fz_rect mediabox;
                fz_matrix transform;
                pdf_page_transform(ctx_, page.get(), &mediabox, &transform);
                return pdf_new_pdf_device(ctx_, doc_, fz_invert_matrix(transform), resources.get(), contents.get());
            }), fz_drop_device);
            const fz_rect rect = toPdf(signature.rect);
            auto buffer = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return fz_new_buffer_from_copied_data(ctx_,
                    reinterpret_cast<const unsigned char *>(signature.png.constData()), signature.png.size());
            }), fz_drop_buffer);
            auto image = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return fz_new_image_from_buffer(ctx_, buffer.get());
            }), fz_drop_image);
            mupdf::call(ctx_, [&]() noexcept {
                const fz_matrix placement = {rect.x1 - rect.x0, 0, 0, rect.y1 - rect.y0, rect.x0, rect.y0};
                fz_fill_image(ctx_, device.get(), image.get(), placement, 1, fz_default_color_params);
                fz_close_device(ctx_, device.get());
                fz_append_string(ctx_, contents.get(), "Q\n");
            });
            auto stream = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
                return pdf_add_stream(ctx_, doc_, contents.get(), nullptr, 0);
            }), pdf_drop_obj);
            const QByteArray id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
            mupdf::call(ctx_, [&]() noexcept {
                pdf_dict_puts_drop(ctx_, stream.get(), signatureIdKey, pdf_new_text_string(ctx_, id.constData()));
                pdf_dict_puts_drop(ctx_, stream.get(), signatureRectKey, pdf_new_rect(ctx_, doc_, rect));
                pdf_array_push(ctx_, streams.get(), stream.get());
            });
        }
        mupdf::call(ctx_, [&]() noexcept {
            pdf_dict_put(ctx_, page->obj, PDF_NAME(Resources), resources.get());
            pdf_dict_put(ctx_, page->obj, PDF_NAME(Contents), streams.get());
            pdf_update_page(ctx_, page.get());
        });
    }
    if (!signatureTemplate.isEmpty()) {
        const QByteArray encoded = signatureTemplate.toBase64();
        mupdf::call(ctx_, [&]() noexcept {
            fz_set_metadata(ctx_, reinterpret_cast<fz_document *>(doc_),
                            signatureMetadata, encoded.constData());
        });
    }
}
