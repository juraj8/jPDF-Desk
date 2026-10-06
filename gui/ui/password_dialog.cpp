#include "ui/password_dialog.h"
#include "ui/dialog_helpers.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <exception>
#include <utility>

PasswordDialog::PasswordDialog(ApplyPassword apply, QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("passwordDialog"));
    setWindowTitle(tr("PDF password"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(dialogHint(tr("Set or replace the password for the next saved copy. The source file is unchanged. "
                                   "Changing encryption may invalidate digital signatures."), this));
    auto *clear = new QCheckBox(tr("Remove password protection"), this);
    clear->setObjectName(QStringLiteral("clearPassword"));
    layout->addWidget(clear);
    auto *form = new QFormLayout;
    layout->addLayout(form);
    const auto passwordField = [this](const QString &name) {
        auto *field = new QLineEdit(this);
        field->setObjectName(name);
        field->setEchoMode(QLineEdit::Password);
        return field;
    };
    auto *password = passwordField(QStringLiteral("newPassword"));
    auto *confirmation = passwordField(QStringLiteral("confirmPassword"));
    form->addRow(tr("New password:"), password);
    form->addRow(tr("Confirm password:"), confirmation);
    connect(clear, &QCheckBox::toggled, this, [=](bool checked) {
        password->setEnabled(!checked);
        confirmation->setEnabled(!checked);
    });
    auto *error = dialogHint({}, this);
    error->setObjectName(QStringLiteral("passwordError"));
    error->setProperty("danger", true);
    layout->addWidget(error);
    auto *buttons = dialogButtons(this, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, clear, password, confirmation, error, apply = std::move(apply)] {
        if (!clear->isChecked() && password->text().isEmpty()) {
            error->setText(tr("Enter a password or select Remove password protection."));
            return;
        }
        if (!clear->isChecked() && password->text() != confirmation->text()) {
            error->setText(tr("Passwords do not match."));
            return;
        }
        try {
            apply(clear->isChecked() ? QString() : password->text());
            accept();
        } catch (const std::exception &e) {
            error->setText(QString::fromUtf8(e.what()));
        }
    });
}
