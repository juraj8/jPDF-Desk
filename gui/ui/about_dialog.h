#pragma once

#include <QDialog>

class QIcon;

class AboutDialog : public QDialog {
public:
    explicit AboutDialog(const QIcon &icon, QWidget *parent = nullptr);
};
