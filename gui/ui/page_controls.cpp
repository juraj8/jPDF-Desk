#include "ui/page_controls.h"
#include "ui/buttons.h"
#include "ui/search_controls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {
// Keep the ordinary box layout's sizing and narrow-window behavior, but use
// spare space to center navigation in the whole bar rather than beside search.
class PageControlsLayout : public QHBoxLayout {
public:
    explicit PageControlsLayout(QWidget *parent) : QHBoxLayout(parent) {}

    void setNavigation(QWidget *previous, QWidget *label, QWidget *next,
                       QWidget *leftEdge, QWidget *rightEdge)
    {
        previous_ = previous;
        label_ = label;
        next_ = next;
        leftEdge_ = leftEdge;
        rightEdge_ = rightEdge;
    }

    void setGeometry(const QRect &rect) override
    {
        QHBoxLayout::setGeometry(rect);
        if (!label_) return;
        const QRect navigation = previous_->geometry().united(next_->geometry());
        const int minimumShift = leftEdge_->geometry().right() + 1 + spacing() - navigation.left();
        const int maximumShift = rightEdge_->geometry().left() - spacing() - 1 - navigation.right();
        if (minimumShift > maximumShift) return;
        const int desiredShift = rect.center().x() - label_->geometry().center().x();
        const int shift = qBound(minimumShift, desiredShift, maximumShift);
        for (auto *widget : {previous_, label_, next_})
            widget->setGeometry(widget->geometry().translated(shift, 0));
    }

private:
    QWidget *previous_ = nullptr;
    QWidget *label_ = nullptr;
    QWidget *next_ = nullptr;
    QWidget *leftEdge_ = nullptr;
    QWidget *rightEdge_ = nullptr;
};
}

PageControls::PageControls(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("pageControls"));
    auto *layout = new PageControlsLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);
    sidebarToggle_ = navigationButton(QStringLiteral("‹"), QStringLiteral("sidebarToggle"),
                                      tr("Hide tools"), this);
    sidebarToggle_->setCheckable(true);
    sidebarToggle_->setChecked(true);
    layout->addWidget(sidebarToggle_);
    layout->addStretch();
    previous_ = navigationButton(QStringLiteral("←"), QStringLiteral("previousButton"),
                                 tr("Previous page"), this);
    label_ = new QLabel(this);
    label_->setObjectName(QStringLiteral("pageLabel"));
    next_ = navigationButton(QStringLiteral("→"), QStringLiteral("nextButton"),
                             tr("Next page"), this);
    layout->addWidget(previous_);
    layout->addWidget(label_);
    layout->addWidget(next_);
    layout->addStretch();
    search_ = new SearchControls(this);
    layout->addWidget(search_);
    outlineToggle_ = navigationButton(QStringLiteral("‹"), QStringLiteral("outlineToggle"),
                                      tr("Open a PDF to view outline and thumbnails"), this);
    outlineToggle_->setCheckable(true);
    outlineToggle_->setEnabled(false);
    layout->addWidget(outlineToggle_);
    layout->setNavigation(previous_, label_, next_, sidebarToggle_, search_);
    setDocumentState(0, 0);
}

void PageControls::setDocumentState(int page, int count)
{
    search_->setDocumentAvailable(count > 0);
    previous_->setEnabled(count > 0 && page > 0);
    next_->setEnabled(count > 0 && page + 1 < count);
    label_->setText(count > 0 ? tr("Page %1 / %2").arg(page + 1).arg(count) : tr("No PDF open"));
}
