#include "ui/editable_text.h"
#include "ui/theme.h"

#include <QCursor>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QTextFrame>

namespace {
void alignPdfBaseline(QTextDocument *document, const QFont &font, float fontSize)
{
    // PdfDocument places the Helvetica baseline at 0.8 * the PDF font size,
    // after its scene-space offset of 4 + 0.125 * fontSize. Use actual Qt
    // font metrics rather than assuming the platform default font's ascent.
    const qreal baseline = 4.0 + fontSize * (0.125 + 0.8 * 1.5);
    auto format = document->rootFrame()->frameFormat();
    format.setTopMargin(baseline - QFontMetricsF(font).ascent());
    document->rootFrame()->setFrameFormat(format);
}
}

EditableText::EditableText(const TextField &field)
{
    setPlainText(field.text);
    setPos(field.rect.topLeft());
    setTextWidth(field.rect.width());
    // Match the PDF FreeText appearance instead of the OS's UI font.
    font_.setFamily(QStringLiteral("Helvetica"));
    font_.setStyleHint(QFont::SansSerif);
    setFontSize(field.fontSize);
    setDefaultTextColor(Qt::black);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusable);
}

void EditableText::setFontSize(int points)
{
    fontSize_ = points;
    font_.setPixelSize(qRound(fontSize_ * 1.5));
    setFont(font_);
    alignPdfBaseline(document(), font_, fontSize_);
}

QRectF EditableText::boundingRect() const
{
    return QGraphicsTextItem::boundingRect().united(QRectF(0, -25, 135, 25));
}

QPainterPath EditableText::shape() const
{
    // Graphics View uses shape(), not boundingRect(), for mouse hit tests.
    QPainterPath path = QGraphicsTextItem::shape();
    if (isSelected())
        path.addRect(QRectF(0, -25, 135, 25));
    return path;
}

void EditableText::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsTextItem::paint(painter, option, widget);
    if (!isSelected()) return;
    painter->setPen(QPen(QColor(UiColors::accent), 1, Qt::DashLine));
    painter->drawRect(QGraphicsTextItem::boundingRect());
    painter->fillRect(QRectF(0, -25, 135, 24), QColor(UiColors::accent));
    painter->setPen(Qt::white);
    painter->drawText(QRectF(5, -25, 125, 24), Qt::AlignVCenter, QObject::tr("✥ Drag to move"));
}

void EditableText::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(isSelected() && event->pos().y() < 0 ? Qt::OpenHandCursor : Qt::IBeamCursor);
    QGraphicsTextItem::hoverMoveEvent(event);
}

void EditableText::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() < 0) {
        dragging_ = true;
        dragOffset_ = event->scenePos() - pos();
        setSelected(true);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsTextItem::mousePressEvent(event);
}

void EditableText::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (dragging_) {
        setPos(event->scenePos() - dragOffset_);
        event->accept();
        return;
    }
    QGraphicsTextItem::mouseMoveEvent(event);
}

void EditableText::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (dragging_) {
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsTextItem::mouseReleaseEvent(event);
}

void EditableText::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->pos().y() < 0) {
        event->accept();
        return;
    }
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void EditableText::focusOutEvent(QFocusEvent *event)
{
    QGraphicsTextItem::focusOutEvent(event);
    setTextInteractionFlags(Qt::NoTextInteraction);
}
