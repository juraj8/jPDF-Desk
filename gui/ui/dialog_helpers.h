#pragma once

#include <QDialogButtonBox>

class QDialog;
class QLabel;
class QString;

// Common dialog primitives. Callers own layout and acceptance/validation policy.
QLabel *dialogHint(const QString &text, QWidget *parent);
QDialogButtonBox *dialogButtons(QDialog *dialog, QDialogButtonBox::StandardButtons buttons);
