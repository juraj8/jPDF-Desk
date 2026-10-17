#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include <QDialog>
#include <functional>

class QVBoxLayout;
class QPushButton;

class SignatureCheckDialog : public QDialog {
public:
    using OnlineCheck = std::function<bool(QWidget *, QVector<DigitalSignatureStatus> &)>;
    using Recheck = std::function<QVector<DigitalSignatureStatus>()>;
    explicit SignatureCheckDialog(const QVector<DigitalSignatureStatus> &signatures,
                                  const QString &trustDescription, QWidget *parent = nullptr,
                                  OnlineCheck onlineCheck = {}, Recheck recheck = {});
private:
    void populateCards();
    void refreshCards();
    QVector<DigitalSignatureStatus> signatures_;
    QWidget *content_ = nullptr;
    QVBoxLayout *cards_ = nullptr;
    QPushButton *online_ = nullptr;
    bool onlineAvailable_ = false;
};
