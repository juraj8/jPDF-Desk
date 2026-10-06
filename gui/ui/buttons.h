#pragma once

#include <QString>

class QPushButton;
class QWidget;

// Consistent sizing and accessibility for compact navigation controls.
QPushButton *navigationButton(const QString &text, const QString &name,
                              const QString &description, QWidget *parent);
