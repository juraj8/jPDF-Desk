#pragma once

#include <QFrame>

class QActionGroup;
class QIcon;
class QLabel;
class QPushButton;
class QString;

// Document actions; the window owns their behavior.
class Sidebar : public QFrame {
public:
    explicit Sidebar(const QIcon &icon, QWidget *parent = nullptr);

    QActionGroup *appearanceActions() const { return appearanceActions_; }
    QPushButton *aboutButton() const { return about_; }
    QPushButton *openButton() const { return open_; }
    QPushButton *addButton() const { return add_; }
    QPushButton *checkButton() const { return check_; }
    QPushButton *crossButton() const { return cross_; }
    QPushButton *loadSignatureButton() const { return loadSignature_; }
    QPushButton *placeSignatureButton() const { return placeSignature_; }
    QPushButton *verifySignaturesButton() const { return verifySignatures_; }
    QPushButton *signButton() const { return sign_; }
    QPushButton *manageCertificatesButton() const { return manageCertificates_; }
    void setSignaturePreview(const QByteArray &png);
    QPushButton *saveButton() const { return save_; }
    QPushButton *printButton() const { return print_; }
    QPushButton *metadataButton() const { return metadata_; }
    QPushButton *passwordButton() const { return password_; }
    QPushButton *formButton() const { return form_; }
    void setDocumentState(int pageCount, bool hasSignature, bool canSign, bool canVerify);

private:
    QActionGroup *appearanceActions_;
    QPushButton *about_;
    QPushButton *open_;
    QPushButton *add_;
    QPushButton *check_;
    QPushButton *cross_;
    QPushButton *loadSignature_;
    QPushButton *placeSignature_;
    QPushButton *save_;
    QPushButton *print_;
    QPushButton *metadata_;
    QPushButton *password_;
    QPushButton *form_;
    QPushButton *sign_;
    QPushButton *verifySignatures_;
    QPushButton *manageCertificates_;
    QLabel *signaturePreview_;
};
