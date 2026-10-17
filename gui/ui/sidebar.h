#pragma once

#include <QFrame>

class QIcon;
class QLabel;
class QPushButton;
class QString;

// Editing and signing tools; the window owns their behavior.
class Sidebar : public QFrame {
public:
    explicit Sidebar(const QIcon &icon, QWidget *parent = nullptr);

    QPushButton *addButton() const { return add_; }
    QPushButton *checkButton() const { return check_; }
    QPushButton *crossButton() const { return cross_; }
    QPushButton *loadSignatureButton() const { return loadSignature_; }
    QPushButton *placeSignatureButton() const { return placeSignature_; }
    QPushButton *verifySignaturesButton() const { return verifySignatures_; }
    QPushButton *signButton() const { return sign_; }
    void setSignaturePreview(const QByteArray &png);
    QPushButton *formButton() const { return form_; }
    void setDocumentState(int pageCount, bool hasSignature, bool canSign, bool canVerify);

private:
    QPushButton *add_;
    QPushButton *check_;
    QPushButton *cross_;
    QPushButton *loadSignature_;
    QPushButton *placeSignature_;
    QPushButton *form_;
    QPushButton *sign_;
    QPushButton *verifySignatures_;
    QLabel *signaturePreview_;
};
