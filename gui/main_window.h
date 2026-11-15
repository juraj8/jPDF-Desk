#pragma once

#include "jpdf_desk/document/pdf_document.h"
#include "signature_store.h"
#include "ui/theme.h"
#include <QMainWindow>

class DocumentView;
class DocumentToolbar;
class PageControls;
class OutlinePanel;
class Sidebar;
struct LoadedPdf;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr, SignatureServices services = {});
    ~MainWindow() override;
    void openDocument(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void applyTheme();
    void openFile();
    bool saveFile(bool digitallySign = false);
    bool confirmUnsavedChanges();
    void checkSignatures();
    void printFile();
    void editMetadata();
    void editForm();
    void refreshThumbnails(const QVector<QImage> &previews = {});
    void editPassword();
    void showDocument(bool resetPage = false, const LoadedPdf *loaded = nullptr);
    void loadSignature();
    void manageCertificates();
    void refreshSignature();

    UiTheme::Mode themeMode_ = UiTheme::loadMode();
    PdfDocument pdf_;
    bool documentModified_ = false;
    DocumentView *view_;
    PageControls *pageControls_;
    OutlinePanel *outlinePanel_;
    Sidebar *sidebar_;
    DocumentToolbar *documentToolbar_;
    QWidget *welcome_;
    QWidget *editor_;
    QByteArray signatureTemplate_;
    SignatureStore signatureStore_{SignatureStore::Kind::Image};
    SignatureStore certificateStore_{SignatureStore::Kind::Certificate};
};
