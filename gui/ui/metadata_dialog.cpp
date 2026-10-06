#include "ui/metadata_dialog.h"
#include "ui/dialog_helpers.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

MetadataDialog::MetadataDialog(const QMap<QString, QString> &metadata, QWidget *parent)
    : QDialog(parent), original_(metadata)
{
    setObjectName(QStringLiteral("metadataDialog"));
    setWindowTitle(tr("File metadata"));
    resize(600, 520);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    layout->addLayout(form);
    const auto addField = [&](const QString &key, const QString &label, bool readOnly = false) {
        auto *field = new QLineEdit(metadata.value(key), this);
        field->setObjectName(QStringLiteral("metadata") + key);
        field->setReadOnly(readOnly);
        form->addRow(label, field);
        if (!readOnly) fields_.insert(key, field);
    };
    addField(QStringLiteral("Title"), tr("Title:"));
    addField(QStringLiteral("Author"), tr("Author:"));
    subject_ = new QPlainTextEdit(metadata.value(QStringLiteral("Subject")), this);
    subject_->setObjectName(QStringLiteral("metadataSubject"));
    subject_->setMinimumHeight(100);
    form->addRow(tr("Subject:"), subject_);
    addField(QStringLiteral("Keywords"), tr("Keywords:"));
    addField(QStringLiteral("Creator"), tr("Creator:"));
    addField(QStringLiteral("Producer"), tr("Producer:"));
    addField(QStringLiteral("CreationDate"), tr("Created (PDF date):"), true);
    addField(QStringLiteral("ModDate"), tr("Modified (PDF date):"), true);
    layout->addWidget(dialogHint(tr("Changes are included when you save the PDF. Editing a signed PDF may invalidate its signatures."), this));
    auto *buttons = dialogButtons(this, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

QMap<QString, QString> MetadataDialog::changes() const
{
    QMap<QString, QString> values;
    for (auto it = fields_.cbegin(); it != fields_.cend(); ++it)
        if (it.value()->text() != original_.value(it.key()))
            values.insert(it.key(), it.value()->text());
    if (subject_->toPlainText() != original_.value(QStringLiteral("Subject")))
        values.insert(QStringLiteral("Subject"), subject_->toPlainText());
    return values;
}
