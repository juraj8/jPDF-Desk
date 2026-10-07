#pragma once

#include "jpdf_desk/document/pdf_types.h"

#include <QFont>
#include <QGraphicsTextItem>

// A PDF text field in scene coordinates, with a selection-only drag handle.
class EditableText : public QGraphicsTextItem {
public:
    enum { Type = QGraphicsItem::UserType + 1 };
    int type() const override { return Type; }
    explicit EditableText(const TextField &field);
    float fontSize() const { return fontSize_; }
    void setFontSize(int points);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QFont font_;
    float fontSize_ = 12;
    QPointF dragOffset_;
    bool dragging_ = false;
};
