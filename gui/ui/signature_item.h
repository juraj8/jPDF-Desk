#pragma once

#include "jpdf_desk/document/pdf_types.h"

#include <QGraphicsPixmapItem>

// New images are movable/scalable. Saved images are selection-only overlays;
// their pixels are already in the page and only removal is supported.
class SignatureItem : public QGraphicsPixmapItem {
public:
    enum { Type = QGraphicsItem::UserType + 3 };
    int type() const override { return Type; }
    explicit SignatureItem(const Signature &signature);
    Signature signature() const;
    bool isSaved() const { return !id_.isEmpty(); }
    int width() const
    {
        return qRound((isSaved() ? savedSize_.width() : pixmap().deviceIndependentSize().width()) * scale());
    }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void setWidth(int width);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QByteArray png_;
    QString id_;
    QSizeF savedSize_;
};
