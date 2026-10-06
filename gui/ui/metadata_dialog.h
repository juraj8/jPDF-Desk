#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

class QLineEdit;
class QPlainTextEdit;

// Edits values only; the caller decides when and where to apply the changes.
class MetadataDialog : public QDialog {
public:
    explicit MetadataDialog(const QMap<QString, QString> &metadata, QWidget *parent = nullptr);
    QMap<QString, QString> changes() const;

private:
    QMap<QString, QString> original_;
    QMap<QString, QLineEdit *> fields_;
    QPlainTextEdit *subject_;
};
