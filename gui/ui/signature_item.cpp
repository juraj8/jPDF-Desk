#include "ui/signature_item.h"
#include "ui/theme.h"

#include <QCursor>
#include <QPainter>

SignatureItem::SignatureItem(const Signature &signature) : png_(signature.png)
{
    const QImage image = QImage::fromData(png_, "PNG");
    setPixmap(QPixmap::fromImage(image));
    // The PNG is mostly transparent. Its default MaskShape only hits ink pixels,
    // making the rest of the signature's bounding box impossible to grab.
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    setTransformationMode(Qt::SmoothTransformation);
    setPos(signature.rect.topLeft());
    if (!image.isNull()) setScale(signature.rect.width() / image.width());
    setFlags(ItemIsSelectable | ItemIsMovable);
    setCursor(QCursor(Qt::OpenHandCursor));
}

Signature SignatureItem::signature() const
{
    // QGraphicsPixmapItem expands boundingRect() for selection outlines;
    // serialize only the image, in page-local coordinates.
    return {mapRectToParent(QRectF(QPointF(), pixmap().deviceIndependentSize())), png_};
}

void SignatureItem::setWidth(int width)
{
    const qreal imageWidth = pixmap().deviceIndependentSize().width();
    if (imageWidth > 0) setScale(qreal(width) / imageWidth);
}

void SignatureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsPixmapItem::paint(painter, option, widget);
    if (isSelected()) {
        painter->setPen(QPen(QColor(UiColors::accent), 1 / scale(), Qt::DashLine));
        painter->drawRect(boundingRect());
    }
}
