#pragma once

#include "pdf_filler/document/pdf_document.h"
#include "signature_store.h"
#include "ui/theme.h"
#include <QMainWindow>

class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class PageControls;
class OutlinePanel;
class QGraphicsView;
class Sidebar;
class TextTools;
class ZoomControls;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr, SignatureServices services = {});
    ~MainWindow() override;
    void openDocument(const QString &path);

private:
    void applyTheme();
    void openFile();
    void saveFile(bool digitallySign = false);
    void checkSignatures();
    void printFile();
    void editMetadata();
    void editPassword();
    void showDocument();
    void refreshPageImages();
    void updateCurrentPage();
    QPointF insertionPoint() const;
    void addText();
    void addMark(OptionMark::Kind kind);
    void loadSignature();
    void manageCertificates();
    void refreshSignature();
    void placeSignature();
    void updateTextTools();
    DocumentAnnotations captureDrafts() const;
    void navigatePage(int offset);
    void goToPage(int pageNumber);
    void clearSearch();
    void findText();
    void navigateMatch(int offset);

    UiTheme::Mode themeMode_ = UiTheme::loadMode();
    PdfDocument pdf_;
    QGraphicsScene *scene_;
    QGraphicsView *view_;
    struct PageView {
        QGraphicsRectItem *root;
        QGraphicsPixmapItem *image;
        qreal resolution = 0;
    };
    QVector<PageView> pages_;
    bool rebuilding_ = false;
    QVector<TextSearchMatch> searchMatches_;
    QVector<QGraphicsRectItem *> searchHighlights_;
    int searchIndex_ = -1;
    PageControls *pageControls_;
    OutlinePanel *outlinePanel_;
    Sidebar *sidebar_;
    TextTools *textTools_;
    ZoomControls *zoomControls_;
    int page_ = 0;
    QByteArray signatureTemplate_;
    SignatureStore signatureStore_{SignatureStore::Kind::Image};
    SignatureStore certificateStore_{SignatureStore::Kind::Certificate};
};
