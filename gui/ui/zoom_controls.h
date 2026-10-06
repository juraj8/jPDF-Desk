#pragma once

#include <QFrame>

class QGraphicsView;
class QPushButton;

// Viewport overlay: stays fixed on screen while the document scrolls and scales.
class ZoomControls : public QFrame {
public:
    explicit ZoomControls(QGraphicsView *view);
    void setDocumentAvailable(bool available);
    QPushButton *zoomInButton() const { return zoomIn_; }
    QPushButton *zoomOutButton() const { return zoomOut_; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void position();
    void zoom(qreal factor);
    void updateButtons();

    QGraphicsView *view_;
    QPushButton *zoomIn_;
    QPushButton *zoomOut_;
    bool available_ = false;
    int wheelDelta_ = 0;
};
