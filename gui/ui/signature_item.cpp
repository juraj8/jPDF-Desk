#include "ui/signature_item.h"
#include "ui/theme.h"

#include <QCursor>
#include <QPainter>
#include <QPainterPath>
#include <QObject>

SignatureItem::SignatureItem(const Signature &signature)
    : png_(signature.png), id_(signature.id), savedSize_(signature.rect.size())
{
    if (isSaved()) {
        setPos(signature.rect.topLeft());
        setFlag(ItemIsSelectable);
        setCursor(QCursor(Qt::PointingHandCursor));
        setToolTip(QObject::tr("Saved signature image — select to remove. Moving and resizing are not supported."));
        return;
    }
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
    const QSizeF size = isSaved() ? savedSize_ : pixmap().deviceIndependentSize();
    return {mapRectToParent(QRectF(QPointF(), size)), png_, id_};
}

void SignatureItem::setWidth(int width)
{
    if (isSaved()) return;
    const qreal imageWidth = pixmap().deviceIndependentSize().width();
    if (imageWidth > 0) setScale(qreal(width) / imageWidth);
}

QRectF SignatureItem::boundingRect() const
{
    return isSaved() ? QRectF(QPointF(), savedSize_) : QGraphicsPixmapItem::boundingRect();
}

QPainterPath SignatureItem::shape() const
{
    if (!isSaved()) return QGraphicsPixmapItem::shape();
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void SignatureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (!isSaved()) QGraphicsPixmapItem::paint(painter, option, widget);
    if (isSelected()) {
        painter->setPen(QPen(QColor(UiColors::accent), 1 / scale(), Qt::DashLine));
        painter->drawRect(boundingRect());
    }
}
