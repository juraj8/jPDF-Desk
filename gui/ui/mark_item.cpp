#include "ui/mark_item.h"
#include "ui/theme.h"

#include <QCursor>
#include <QPainter>

MarkItem::MarkItem(const OptionMark &mark) : kind_(mark.kind)
{
    setPos(mark.center);
    setFlags(ItemIsSelectable | ItemIsMovable);
    setCursor(QCursor(Qt::OpenHandCursor));
}

void MarkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind_ == OptionMark::Check) {
        QPolygonF stroke;
        stroke << QPointF(-9.6, 0) << QPointF(-2.4, 7.2) << QPointF(10.8, -8.4);
        painter->drawPolyline(stroke);
    } else {
        painter->drawLine(QPointF(-8.4, -8.4), QPointF(8.4, 8.4));
        painter->drawLine(QPointF(8.4, -8.4), QPointF(-8.4, 8.4));
    }
    if (isSelected()) {
        painter->setPen(QPen(QColor(UiColors::accent), 1, Qt::DashLine));
        painter->drawRect(boundingRect());
    }
}
