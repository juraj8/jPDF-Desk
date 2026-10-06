#include "ui/buttons.h"

#include <QPushButton>

QPushButton *navigationButton(const QString &text, const QString &name,
                              const QString &description, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(name);
    button->setFixedSize(36, 36);
    button->setToolTip(description);
    button->setAccessibleName(description);
    return button;
}
