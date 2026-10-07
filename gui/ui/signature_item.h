#pragma once

#include "jpdf_desk/document/pdf_types.h"

#include <QGraphicsPixmapItem>

// A movable, scalable image; keep the PNG on each item so replacing the template
// does not alter signatures already placed on other pages.
class SignatureItem : public QGraphicsPixmapItem {
public:
    enum { Type = QGraphicsItem::UserType + 3 };
    int type() const override { return Type; }
    explicit SignatureItem(const Signature &signature);
    Signature signature() const;
    int width() const { return qRound(pixmap().deviceIndependentSize().width() * scale()); }
    void setWidth(int width);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QByteArray png_;
};
