#pragma once

#include "jpdf_desk/document/pdf_document.h"
#include <QGraphicsView>

class QGraphicsPixmapItem;
class QGraphicsRectItem;
class TextTools;
class ZoomControls;
struct LoadedPdf;

// The document must outlive the view. File operations remain with the caller;
// scene annotations are drafts until captureDrafts() is passed to a save/print.
class DocumentView : public QGraphicsView {
    Q_OBJECT
public:
    explicit DocumentView(PdfDocument &document, QWidget *parent = nullptr);
    ~DocumentView() override;

    void showDocument(bool resetPage = false, const LoadedPdf *loaded = nullptr);
    void setDarkTheme(bool dark);
    void refreshFormValues();
    int currentPage() const { return page_; }
    DocumentAnnotations captureDrafts() const;
    void navigatePage(int offset);
    void goToPage(int page);
    void addText();
    void addMark(OptionMark::Kind kind);
    void placeSignature(const QByteArray &png);
    void clearSearch();
    void findText(const QString &query);
    void navigateMatch(int offset);

signals:
    void currentPageChanged(int page, int count);
    void searchResultChanged(int index, int count);
    void pdfError(const QString &message);
    void searchError(const QString &message);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void refreshPageImages();
    void updateCurrentPage();
    void updateTextTools();
    QPointF insertionPoint() const;

    PdfDocument &pdf_;
    QGraphicsScene *scene_;
    struct PageView {
        QGraphicsRectItem *root;
        QGraphicsPixmapItem *image;
        qreal resolution = 0;
    };
    QVector<PageView> pages_;
    bool rebuilding_ = false;
    bool dark_ = false;
    int page_ = 0;
    QVector<TextSearchMatch> searchMatches_;
    QVector<QGraphicsRectItem *> searchHighlights_;
    int searchIndex_ = -1;
    TextTools *textTools_;
    ZoomControls *zoomControls_;
};
