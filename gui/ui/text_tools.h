#pragma once

#include <QFrame>

class QGraphicsItem;
class QGraphicsView;
class QLabel;
class QPushButton;
class QSpinBox;

// Contextual controls anchored to the selected field inside the view's viewport.
class TextTools : public QFrame {
public:
    explicit TextTools(QGraphicsView *view);
    QSpinBox *sizeControl() const { return size_; }
    QSpinBox *widthControl() const { return width_; }
    QPushButton *removeButton() const { return remove_; }
    void positionFor(const QGraphicsItem *item);
    void setSelectionType(bool text, bool signature);

private:
    QGraphicsView *view_;
    QLabel *sizeLabel_;
    QSpinBox *size_;
    QLabel *widthLabel_;
    QSpinBox *width_;
    QPushButton *remove_;
};
