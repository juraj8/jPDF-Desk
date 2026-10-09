#include "jpdf_desk/document/pdf_document.h"
#include "jpdf_desk/document/detail/mupdf_call.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QSet>
#include <utility>

namespace {
pdf_obj *fieldOwner(fz_context *ctx, pdf_obj *widget)
{
    // A terminal field may own several unnamed widget children.
    for (int depth = 0; depth < 100; ++depth) {
        if (pdf_dict_get(ctx, widget, PDF_NAME(T))) return widget;
        pdf_obj *parent = pdf_dict_get(ctx, widget, PDF_NAME(Parent));
        if (!parent) return widget;
        widget = parent;
    }
    fz_throw(ctx, FZ_ERROR_FORMAT, "Recursive form field hierarchy");
}
}

QVector<PdfFormField> PdfDocument::formFields(int pageNumber) const
{
    QVector<PdfFormField> result;
    if (!doc_) return result;
    auto page = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
        return pdf_load_page(ctx_, doc_, pageNumber);
    }), pdf_drop_page);
    QSet<int> seen;
    for (auto *widget = pdf_first_widget(ctx_, page.get()); widget;
         widget = pdf_next_widget(ctx_, widget)) {
        auto *field = mupdf::call(ctx_, [&]() noexcept {
            return fieldOwner(ctx_, pdf_annot_obj(ctx_, widget));
        });
        const int id = mupdf::call(ctx_, [&]() noexcept { return pdf_to_num(ctx_, field); });
        if (seen.contains(id)) continue;
        seen.insert(id);
        PdfFormField value;
        value.id = id;
        auto name = mupdf::own(ctx_, mupdf::call(ctx_, [&]() noexcept {
            return pdf_load_field_name(ctx_, field);
        }), fz_free);
        value.name = QString::fromUtf8(name.get());
        value.value = QString::fromUtf8(mupdf::call(ctx_, [&]() noexcept {
            return pdf_field_value(ctx_, field);
        }));
        const int flags = mupdf::call(ctx_, [&]() noexcept { return pdf_field_flags(ctx_, field); });
        value.readOnly = id <= 0 || mupdf::call(ctx_, [&]() noexcept {
            return pdf_widget_is_readonly(ctx_, widget);
        });
        const int type = mupdf::call(ctx_, [&]() noexcept { return pdf_widget_type(ctx_, widget); });
        if (type == PDF_WIDGET_TYPE_TEXT) {
            value.kind = PdfFormField::Kind::Text;
            value.multiline = flags & PDF_TX_FIELD_IS_MULTILINE;
            value.password = flags & PDF_TX_FIELD_IS_PASSWORD;
            value.maxLength = mupdf::call(ctx_, [&]() noexcept { return pdf_text_widget_max_len(ctx_, widget); });
        } else if (type == PDF_WIDGET_TYPE_CHECKBOX) {
            value.kind = PdfFormField::Kind::CheckBox;
            value.onValue = QString::fromUtf8(mupdf::call(ctx_, [&]() noexcept {
                return pdf_to_name(ctx_, pdf_button_field_on_state(ctx_, field));
            }));
            if (value.onValue.isEmpty() || value.onValue == QStringLiteral("Off")) value.readOnly = true;
        } else if ((type == PDF_WIDGET_TYPE_COMBOBOX || type == PDF_WIDGET_TYPE_LISTBOX)
                   && !(flags & PDF_CH_FIELD_IS_MULTI_SELECT)) {
            value.kind = PdfFormField::Kind::Choice;
            value.editableChoice = type == PDF_WIDGET_TYPE_COMBOBOX && (flags & PDF_CH_FIELD_IS_EDIT);
            const int count = mupdf::call(ctx_, [&]() noexcept {
                return pdf_choice_field_option_count(ctx_, field);
            });
            for (int i = 0; i < count; ++i) {
                const QString exported = QString::fromUtf8(mupdf::call(ctx_, [&]() noexcept {
                    return pdf_choice_field_option(ctx_, field, 1, i);
                }));
                const QString label = QString::fromUtf8(mupdf::call(ctx_, [&]() noexcept {
                    return pdf_choice_field_option(ctx_, field, 0, i);
                }));
                value.options.append({exported, label});
            }
        }
        result.append(std::move(value));
    }
    return result;
}

void PdfDocument::setFormValues(const QMap<int, QString> &values)
{
    if (values.isEmpty()) return;
    QMap<int, PdfFormField> fields;
    for (int page = 0; page < pageCount(); ++page)
        for (const auto &field : formFields(page)) fields.insert(field.id, field);
    // Validate the whole request before modifying a copy. Never write arbitrary objects.
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!fields.contains(it.key())) throw std::runtime_error("Unknown PDF form field.");
        const auto &field = fields[it.key()];
        const QString &value = it.value();
        if (field.readOnly || field.kind == PdfFormField::Kind::Unsupported)
            throw std::runtime_error("This PDF form field is read-only or unsupported.");
        if (value.contains(QChar(0))) throw std::runtime_error("Form values cannot contain null characters.");
        if (field.kind == PdfFormField::Kind::Text && field.maxLength > 0
            && value.toUcs4().size() > field.maxLength)
            throw std::runtime_error("The text exceeds the PDF field's maximum length.");
        if (field.kind == PdfFormField::Kind::CheckBox
            && value != QStringLiteral("Off") && value != field.onValue)
            throw std::runtime_error("Invalid checkbox value.");
        if (field.kind == PdfFormField::Kind::Choice && !field.editableChoice) {
            bool found = value.isEmpty();
            for (const auto &option : field.options) found = found || option.first == value;
            if (!found) throw std::runtime_error("Invalid PDF choice value.");
        }
    }
    auto copy = snapshot({});
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        auto field = mupdf::own(copy->ctx_, mupdf::call(copy->ctx_, [&]() noexcept {
            return pdf_load_object(copy->ctx_, copy->doc_, it.key());
        }), pdf_drop_obj);
        const QByteArray text = it.value().toUtf8();
        const bool choice = fields.value(it.key()).kind == PdfFormField::Kind::Choice;
        mupdf::call(copy->ctx_, [&]() noexcept {
            if (!pdf_set_field_value(copy->ctx_, copy->doc_, field.get(), text.constData(), 1))
                fz_throw(copy->ctx_, FZ_ERROR_ARGUMENT, "Cannot update PDF form field");
            // A previous choice selection index must not override the new /V.
            if (choice)
                pdf_dict_del(copy->ctx_, field.get(), PDF_NAME(I));
        });
    }
    // Generate widget appearances before adoption, so errors leave the live PDF intact.
    for (int i = 0; i < copy->pageCount(); ++i) {
        auto page = mupdf::own(copy->ctx_, mupdf::call(copy->ctx_, [&]() noexcept {
            return pdf_load_page(copy->ctx_, copy->doc_, i);
        }), pdf_drop_page);
        mupdf::call(copy->ctx_, [&]() noexcept { pdf_update_page(copy->ctx_, page.get()); });
    }
    std::swap(ctx_, copy->ctx_);
    std::swap(doc_, copy->doc_);
}
