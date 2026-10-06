#include "ui/text_tools.h"

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

TextTools::TextTools(QGraphicsView *view) : QFrame(view->viewport()), view_(view)
{
    setObjectName(QStringLiteral("textTools"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    sizeLabel_ = new QLabel(tr("Text size"), this);
    layout->addWidget(sizeLabel_);
    size_ = new QSpinBox(this);
    size_->setObjectName(QStringLiteral("textSize"));
    size_->setRange(6, 72);
    size_->setValue(12);
    size_->setSuffix(tr(" pt"));
    size_->setToolTip(tr("Font size for the selected text"));
    layout->addWidget(size_);
    widthLabel_ = new QLabel(tr("Width"), this);
    layout->addWidget(widthLabel_);
    width_ = new QSpinBox(this);
    width_->setObjectName(QStringLiteral("signatureWidth"));
    width_->setRange(40, 600);
    width_->setSuffix(tr(" px"));
    layout->addWidget(width_);
    widthLabel_->hide();
    width_->hide();
    remove_ = new QPushButton(tr("Delete text"), this);
    remove_->setObjectName(QStringLiteral("removeButton"));
    remove_->setProperty("danger", true);
    layout->addWidget(remove_);
    adjustSize();
    hide();
}

void TextTools::setSelectionType(bool text, bool signature)
{
    sizeLabel_->setVisible(text);
    size_->setVisible(text);
    widthLabel_->setVisible(signature);
    width_->setVisible(signature);
    remove_->setText(signature ? tr("Delete signature") : text ? tr("Delete text") : tr("Delete mark"));
    adjustSize();
}

void TextTools::positionFor(const QGraphicsItem *item)
{
    const QRectF box = item->sceneBoundingRect();
    const QPoint topLeft = view_->mapFromScene(box.topLeft());
    const QPoint bottomLeft = view_->mapFromScene(box.bottomLeft());
    const QSize size = sizeHint();
    const int x = qBound(0, topLeft.x(), qMax(0, view_->viewport()->width() - size.width()));
    const int above = topLeft.y() - size.height() - 4;
    const int y = above >= 0 ? above : qMin(bottomLeft.y() + 4,
                                           qMax(0, view_->viewport()->height() - size.height()));
    setGeometry(x, y, size.width(), size.height());
    show();
    raise();
}
