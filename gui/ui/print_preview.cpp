#include "ui/print_preview.h"

#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPrintPreviewWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <exception>

PrintPreview::PrintPreview(Renderer renderer, ErrorHandler onError, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("printPreviewPanel"));
    printer_.setOutputFormat(QPrinter::PdfFormat);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    view_ = new QPrintPreviewWidget(&printer_, this);
    view_->setObjectName(QStringLiteral("printPreview"));
    layout->addWidget(view_, 1);
    auto *navigation = new QHBoxLayout;
    layout->addLayout(navigation);
    auto *page = new QSpinBox(this);
    page->setObjectName(QStringLiteral("printPreviewPage"));
    page->setPrefix(tr("Page "));
    auto *count = new QLabel(this);
    auto *zoomOut = new QPushButton(tr("−"), this);
    zoomOut->setToolTip(tr("Zoom out"));
    zoomOut->setAccessibleName(tr("Zoom out"));
    auto *zoomIn = new QPushButton(tr("+"), this);
    zoomIn->setToolTip(tr("Zoom in"));
    zoomIn->setAccessibleName(tr("Zoom in"));
    zoom_ = new QSpinBox(this);
    zoom_->setObjectName(QStringLiteral("printPreviewZoom"));
    zoom_->setAccessibleName(tr("Preview zoom percentage"));
    zoom_->setRange(1, 1000);
    zoom_->setSuffix(QStringLiteral(" %"));
    zoom_->setToolTip(tr("Zoom the preview, or use Ctrl + mouse wheel. This does not change print scaling."));
    if (auto *graphics = view_->findChild<QGraphicsView *>())
        graphics->viewport()->installEventFilter(this);
    auto *fitPage = new QPushButton(tr("Fit page"), this);
    fitPage->setObjectName(QStringLiteral("printPreviewFitPage"));
    auto *fitWidth = new QPushButton(tr("Fit width"), this);
    fitWidth->setObjectName(QStringLiteral("printPreviewFitWidth"));
    navigation->addWidget(page);
    navigation->addWidget(count);
    navigation->addStretch();
    navigation->addWidget(zoomOut);
    navigation->addWidget(zoom_);
    navigation->addWidget(zoomIn);
    navigation->addWidget(fitPage);
    navigation->addWidget(fitWidth);
    connect(page, &QSpinBox::valueChanged, view_, &QPrintPreviewWidget::setCurrentPage);
    connect(zoomOut, &QPushButton::clicked, this, [this] { setZoom(view_->zoomFactor() / 1.2); });
    connect(zoomIn, &QPushButton::clicked, this, [this] { setZoom(view_->zoomFactor() * 1.2); });
    connect(zoom_, &QSpinBox::valueChanged, this, [this](int percent) { setZoom(percent / 100.0); });
    connect(fitPage, &QPushButton::clicked, view_, &QPrintPreviewWidget::fitInView);
    connect(fitWidth, &QPushButton::clicked, view_, &QPrintPreviewWidget::fitToWidth);
    connect(view_, &QPrintPreviewWidget::previewChanged, this, [this, page, count] {
        const QSignalBlocker pageBlocker(page);
        page->setRange(1, qMax(1, view_->pageCount()));
        page->setValue(view_->currentPage());
        count->setText(tr("of %1").arg(view_->pageCount()));
        const QSignalBlocker zoomBlocker(zoom_);
        zoom_->setValue(qRound(view_->zoomFactor() * 100));
    });
    connect(view_, &QPrintPreviewWidget::paintRequested, this,
            [this, renderer, onError](QPrinter *target) {
        if (!renderer) return;
        try {
            renderer(*target, options_);
        } catch (const std::exception &e) {
            if (onError) onError(tr("Cannot preview PDF: %1").arg(QString::fromUtf8(e.what())));
        }
    });
}

PrintPreview::~PrintPreview()
{
    // QPrintPreviewWidget must be destroyed before the printer it references.
    delete view_;
}

QString PrintPreview::configure(const PrintSettings &settings, int pageCount, int currentPage)
{
    const QString error = settings.apply(printer_, pageCount, currentPage, PrintSettings::Purpose::Preview);
    if (error.isEmpty()) options_ = settings.options;
    return error;
}

void PrintPreview::refresh()
{
    view_->updatePreview();
    view_->fitInView();
}

void PrintPreview::setZoom(qreal factor)
{
    view_->setZoomFactor(qBound(0.01, factor, 10.0));
    const QSignalBlocker blocker(zoom_);
    zoom_->setValue(qRound(view_->zoomFactor() * 100));
}

bool PrintPreview::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier) && wheel->angleDelta().y() != 0) {
            setZoom(view_->zoomFactor() * (wheel->angleDelta().y() > 0 ? 1.2 : 1 / 1.2));
            wheel->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
