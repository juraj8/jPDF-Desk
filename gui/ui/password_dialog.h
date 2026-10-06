#pragma once

#include <QDialog>
#include <QString>
#include <functional>

// The callback applies the requested change, or throws to keep the dialog open
// with an inline error. No document or persistence dependency belongs here.
class PasswordDialog : public QDialog {
public:
    using ApplyPassword = std::function<void(const QString &)>;
    explicit PasswordDialog(ApplyPassword apply, QWidget *parent = nullptr);
};
