#include "ui/zoom_controls.h"
#include "ui/buttons.h"

#include <QEvent>
#include <QGraphicsView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
constexpr qreal minimumZoom = 0.25;
constexpr qreal maximumZoom = 4.0;
}

ZoomControls::ZoomControls(QGraphicsView *view)
    : QFrame(view), view_(view)
{
    setObjectName(QStringLiteral("zoomControls"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    zoomIn_ = navigationButton(QStringLiteral("+"), QStringLiteral("zoomInButton"), tr("Zoom in"), this);
    zoomOut_ = navigationButton(QStringLiteral("−"), QStringLiteral("zoomOutButton"), tr("Zoom out"), this);
    for (auto *button : {zoomIn_, zoomOut_})
        layout->addWidget(button);
    connect(zoomIn_, &QPushButton::clicked, this, [this] { zoom(1.25); });
    connect(zoomOut_, &QPushButton::clicked, this, [this] { zoom(1.0 / 1.25); });
    view_->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    view_->viewport()->installEventFilter(this);
    adjustSize();
    position();
    updateButtons();
}

void ZoomControls::setDocumentAvailable(bool available)
{
    available_ = available;
    updateButtons();
}

void ZoomControls::zoom(qreal factor)
{
    if (!available_) return;
    const qreal current = view_->transform().m11();
    const qreal target = qBound(minimumZoom, current * factor, maximumZoom);
    view_->scale(target / current, target / current);
    updateButtons();
}

void ZoomControls::updateButtons()
{
    const qreal scale = view_->transform().m11();
    zoomIn_->setEnabled(available_ && scale < maximumZoom - 0.0001);
    zoomOut_->setEnabled(available_ && scale > minimumZoom + 0.0001);
}

void ZoomControls::position()
{
    constexpr int margin = 16;
    const QPoint origin = view_->viewport()->pos();
    move(origin.x() + qMax(0, view_->viewport()->width() - width() - margin),
         origin.y() + qMax(0, view_->viewport()->height() - height() - margin));
    raise();
}

bool ZoomControls::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == view_->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Move))
        position();
    if (watched == view_->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            // Accumulate partial wheel ticks from high-resolution devices.
            wheelDelta_ += wheel->angleDelta().y();
            while (wheelDelta_ >= 120) {
                zoomIn_->click();
                wheelDelta_ -= 120;
            }
            while (wheelDelta_ <= -120) {
                zoomOut_->click();
                wheelDelta_ += 120;
            }
            wheel->accept();
            return true;
        }
        wheelDelta_ = 0;
    }
    return QFrame::eventFilter(watched, event);
}
