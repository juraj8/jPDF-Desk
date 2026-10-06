#include "ui/metadata_dialog.h"
#include "ui/password_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <stdexcept>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MetadataDialog metadata({{"Title", "Original"}, {"Subject", "Subject"},
                             {"CreationDate", "D:20250101"}});
    if (!metadata.changes().isEmpty()) return 1;
    auto *title = metadata.findChild<QLineEdit *>("metadataTitle");
    auto *created = metadata.findChild<QLineEdit *>("metadataCreationDate");
    auto *subject = metadata.findChild<QPlainTextEdit *>("metadataSubject");
    if (!title || !created || !subject || !created->isReadOnly()) return 2;
    title->clear(); // Empty values must be included, not treated as unchanged.
    subject->setPlainText("Updated\nsubject");
    if (metadata.changes() != QMap<QString, QString>{{"Title", ""}, {"Subject", "Updated\nsubject"}})
        return 3;
    title->setText("Original");
    subject->setPlainText("Subject");
    if (!metadata.changes().isEmpty()) return 4;

    int calls = 0;
    QString applied;
    bool fail = true;
    PasswordDialog password([&](const QString &value) {
        ++calls;
        if (fail) throw std::runtime_error("Rejected by document");
        applied = value;
    });
    auto *entry = password.findChild<QLineEdit *>("newPassword");
    auto *confirmation = password.findChild<QLineEdit *>("confirmPassword");
    auto *clear = password.findChild<QCheckBox *>("clearPassword");
    auto *error = password.findChild<QLabel *>("passwordError");
    auto *buttons = password.findChild<QDialogButtonBox *>();
    if (!entry || !confirmation || !clear || !error || !buttons) return 5;
    password.show();
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    ok->click();
    if (calls || error->text().isEmpty()) return 6;
    entry->setText("secret");
    confirmation->setText("different");
    ok->click();
    if (calls || error->text().isEmpty()) return 7;
    confirmation->setText("secret");
    ok->click();
    if (calls != 1 || !password.isVisible() || error->text() != "Rejected by document") return 8;
    if (error->textFormat() != Qt::PlainText || !error->property("danger").toBool()) return 9;
    fail = false;
    ok->click();
    if (calls != 2 || applied != "secret" || password.result() != QDialog::Accepted) return 10;

    PasswordDialog removal([&](const QString &value) { applied = value; });
    removal.findChild<QCheckBox *>("clearPassword")->setChecked(true);
    if (removal.findChild<QLineEdit *>("newPassword")->isEnabled()
        || removal.findChild<QLineEdit *>("confirmPassword")->isEnabled()) return 11;
    removal.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    if (!applied.isEmpty() || removal.result() != QDialog::Accepted) return 12;

    PasswordDialog cancelled([&](const QString &) { ++calls; });
    cancelled.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
    if (calls != 2 || cancelled.result() != QDialog::Rejected) return 13;
    return 0;
}
