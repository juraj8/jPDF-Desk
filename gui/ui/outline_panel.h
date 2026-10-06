#pragma once

#include "pdf_filler/document/pdf_types.h"
#include <QFrame>
#include <QImage>
#include <functional>

class QPushButton;
class QTreeWidget;
class QListWidget;
class QStackedWidget;

// PDF bookmarks and lazy page previews. Navigation is owned by MainWindow.
class OutlinePanel : public QFrame {
public:
    explicit OutlinePanel(QPushButton *toggle, QWidget *parent = nullptr);
    void setEntries(const QVector<OutlineEntry> &entries);
    QTreeWidget *tree() const { return tree_; }
    void setPages(int count, std::function<QImage(int)> render);
    void setCurrentPage(int page);
    QListWidget *thumbnails() const { return thumbnails_; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateExpandedState();
    void refreshThumbnails();
    QPushButton *toggle_;
    QTreeWidget *tree_;
    QFrame *outlineCard_;
    QStackedWidget *views_;
    QPushButton *outlineViewButton_;
    QPushButton *thumbnailViewButton_;
    QListWidget *thumbnails_;
    std::function<QImage(int)> render_;
    bool expanded_ = true;
    bool showOutline_ = true;
};
