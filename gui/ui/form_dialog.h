#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include <QDialog>
#include <functional>

// Value-only AcroForm editor; applying changes belongs to the caller.
class FormDialog : public QDialog {
public:
    explicit FormDialog(const QVector<PdfFormField> &fields, int page,
                        std::function<void(const QMap<int, QString> &)> apply,
                        QWidget *parent = nullptr);
    QMap<int, QString> changes() const;

private:
    struct Editor {
        PdfFormField field;
        std::function<QString()> value;
    };
    QVector<Editor> editors_;
};
