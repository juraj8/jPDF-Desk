#pragma once

#include <QFrame>

class QAction;
class QActionGroup;

// Document actions share a compact overflow menu beside page navigation.
class DocumentToolbar : public QFrame {
public:
    explicit DocumentToolbar(QWidget *parent = nullptr);
    QActionGroup *appearanceActions() const { return appearanceActions_; }
    QAction *aboutAction() const { return about_; }
    QAction *openAction() const { return open_; }
    QAction *saveAction() const { return save_; }
    QAction *printAction() const { return print_; }
    QAction *metadataAction() const { return metadata_; }
    QAction *passwordAction() const { return password_; }
    QAction *manageSignatureImagesAction() const { return manageSignatureImages_; }
    QAction *manageCertificatesAction() const { return manageCertificates_; }
    void setDocumentState(bool hasDocument);

private:
    QActionGroup *appearanceActions_;
    QAction *about_;
    QAction *open_;
    QAction *save_;
    QAction *print_;
    QAction *metadata_;
    QAction *password_;
    QAction *manageSignatureImages_;
    QAction *manageCertificates_;
};
