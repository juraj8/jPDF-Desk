#include "ui/dialog_helpers.h"

#include <QDialog>
#include <QLabel>
#include <QIcon>
#include <QPushButton>

QLabel *dialogHint(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

QDialogButtonBox *dialogButtons(QDialog *dialog, QDialogButtonBox::StandardButtons buttons)
{
    auto *box = new QDialogButtonBox(buttons, dialog);
    if (auto *close = box->button(QDialogButtonBox::Close))
        close->setIcon(QIcon(QStringLiteral(":/ui/close-red.xpm")));
    QObject::connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return box;
}
