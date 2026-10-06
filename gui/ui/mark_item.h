#pragma once

#include "pdf_filler/document/pdf_types.h"

#include <QGraphicsItem>

// A movable mark placed over a printed option box. PDF export draws the same strokes as ink.
class MarkItem : public QGraphicsItem {
public:
    enum { Type = QGraphicsItem::UserType + 2 };
    int type() const override { return Type; }
    explicit MarkItem(const OptionMark &mark);
    OptionMark mark() const { return {kind_, pos()}; }

    QRectF boundingRect() const override { return QRectF(-12, -12, 24, 24); }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

private:
    OptionMark::Kind kind_;
};
