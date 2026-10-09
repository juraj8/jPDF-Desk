#include "ui/form_dialog.h"
#include "ui/dialog_helpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <exception>

FormDialog::FormDialog(const QVector<PdfFormField> &fields, int page,
                       std::function<void(const QMap<int, QString> &)> apply, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("formDialog"));
    setWindowTitle(tr("Form fields — Page %1").arg(page + 1));
    resize(600, 480);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(dialogHint(tr("Edit native PDF fields on the current page. Changes are included when you save or print. Editing may invalidate digital signatures."), this));
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll);
    auto *form = new QFormLayout(body);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    if (fields.isEmpty()) form->addRow(dialogHint(tr("No native form fields on this page. Use the Text and Checkmark tools for a flat or scanned form."), body));
    for (const auto &field : fields) {
        QWidget *control = nullptr;
        std::function<QString()> value;
        if (field.kind == PdfFormField::Kind::Text && field.multiline && !field.password) {
            auto *text = new QPlainTextEdit(field.value, body);
            text->setMaximumHeight(120);
            control = text;
            value = [text] { return text->toPlainText(); };
        } else if (field.kind == PdfFormField::Kind::Text) {
            auto *text = new QLineEdit(field.value, body);
            if (field.password) text->setEchoMode(QLineEdit::Password);
            control = text;
            value = [text] { return text->text(); };
        } else if (field.kind == PdfFormField::Kind::CheckBox) {
            auto *check = new QCheckBox(tr("Checked"), body);
            check->setChecked(field.value == field.onValue);
            control = check;
            value = [check, on = field.onValue] { return check->isChecked() ? on : QStringLiteral("Off"); };
        } else if (field.kind == PdfFormField::Kind::Choice) {
            auto *choice = new QComboBox(body);
            choice->setEditable(field.editableChoice);
            choice->addItem(tr("No selection"), QString());
            for (const auto &option : field.options) choice->addItem(option.second, option.first);
            int index = choice->findData(field.value);
            if (index < 0) { // Preserve an existing nonstandard value unless changed.
                choice->addItem(field.value, field.value);
                index = choice->count() - 1;
            }
            choice->setCurrentIndex(index);
            control = choice;
            value = [choice] {
                if (choice->isEditable() && choice->currentText() != choice->itemText(choice->currentIndex()))
                    return choice->currentText();
                return choice->currentData().toString();
            };
        } else {
            auto *label = new QLabel(tr("Unsupported field type"), body);
            control = label;
        }
        control->setObjectName(QStringLiteral("formField_%1").arg(field.id));
        control->setEnabled(!field.readOnly && bool(value));
        control->setAccessibleName(field.name);
        control->setToolTip(field.readOnly ? tr("Read-only field") : field.name);
        auto *label = new QLabel(field.name.isEmpty() ? tr("Unnamed field") : field.name, body);
        label->setTextFormat(Qt::PlainText);
        label->setBuddy(control);
        form->addRow(label, control);
        if (value && !field.readOnly) editors_.append({field, std::move(value)});
    }
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    layout->addWidget(dialogHint(tr("Radio buttons, multi-select lists, XFA forms, JavaScript validation/calculation and submit actions are not supported."), this));
    auto *error = dialogHint({}, this);
    error->setObjectName(QStringLiteral("formError"));
    layout->addWidget(error);
    auto *buttons = dialogButtons(this, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, apply = std::move(apply), error] {
        try {
            apply(changes());
            accept();
        } catch (const std::exception &e) {
            error->setText(QString::fromUtf8(e.what()));
        }
    });
}

QMap<int, QString> FormDialog::changes() const
{
    QMap<int, QString> result;
    for (const auto &editor : editors_) {
        const QString value = editor.value();
        if (value != editor.field.value) result.insert(editor.field.id, value);
    }
    return result;
}
