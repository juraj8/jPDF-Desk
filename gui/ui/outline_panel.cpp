#include "ui/outline_panel.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QEvent>
#include <QListWidget>
#include <QPixmap>
#include <QScrollBar>
#include <QTimer>
#include <QPushButton>
#include <exception>
#include <utility>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
void addEntries(QTreeWidget *tree, QTreeWidgetItem *parent,
                const QVector<OutlineEntry> &entries)
{
    for (const auto &entry : entries) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        item->setText(0, entry.title.isEmpty() ? QObject::tr("Untitled bookmark") : entry.title);
        item->setData(0, Qt::UserRole, entry.page);
        item->setToolTip(0, entry.page >= 0
            ? QObject::tr("%1 — Page %2").arg(entry.title).arg(entry.page + 1)
            : QObject::tr("%1 — No internal page destination").arg(entry.title));
        addEntries(tree, item, entry.children);
        item->setExpanded(entry.expanded);
    }
}
}

OutlinePanel::OutlinePanel(QPushButton *toggle, QWidget *parent)
    : QFrame(parent), toggle_(toggle)
{
    setObjectName(QStringLiteral("outlinePanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 8);
    layout->setSpacing(8);
    setFixedWidth(260);
    auto *buttons = new QHBoxLayout;
    outlineViewButton_ = new QPushButton(tr("Outline"), this);
    outlineViewButton_->setObjectName(QStringLiteral("outlineViewButton"));
    thumbnailViewButton_ = new QPushButton(tr("Thumbnails"), this);
    thumbnailViewButton_->setObjectName(QStringLiteral("thumbnailViewButton"));
    auto *group = new QButtonGroup(this);
    for (auto *button : {outlineViewButton_, thumbnailViewButton_}) {
        button->setCheckable(true);
        group->addButton(button);
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    views_ = new QStackedWidget(this);
    layout->addWidget(views_, 1);
    connect(outlineViewButton_, &QPushButton::clicked, this, [this] {
        showOutline_ = true;
        updateExpandedState();
    });
    connect(thumbnailViewButton_, &QPushButton::clicked, this, [this] {
        showOutline_ = false;
        updateExpandedState();
    });
    outlineCard_ = new QFrame(this);
    outlineCard_->setObjectName(QStringLiteral("outlineCard"));
    auto *outlineLayout = new QVBoxLayout(outlineCard_);
    auto *heading = new QLabel(tr("OUTLINE"), outlineCard_);
    heading->setObjectName(QStringLiteral("sectionHeading"));
    outlineLayout->addWidget(heading);
    tree_ = new QTreeWidget(outlineCard_);
    tree_->setObjectName(QStringLiteral("outlineTree"));
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setAccessibleName(tr("PDF outline"));
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    outlineLayout->addWidget(tree_, 1);
    views_->addWidget(outlineCard_);

    auto *thumbnailCard = new QFrame(this);
    thumbnailCard->setObjectName(QStringLiteral("thumbnailCard"));
    auto *thumbnailLayout = new QVBoxLayout(thumbnailCard);
    auto *thumbnailHeading = new QLabel(tr("THUMBNAILS"), thumbnailCard);
    thumbnailHeading->setObjectName(QStringLiteral("sectionHeading"));
    thumbnailLayout->addWidget(thumbnailHeading);
    thumbnails_ = new QListWidget(thumbnailCard);
    thumbnails_->setObjectName(QStringLiteral("pageThumbnails"));
    thumbnails_->setAccessibleName(tr("PDF page thumbnails"));
    thumbnails_->setViewMode(QListView::IconMode);
    thumbnails_->setFlow(QListView::TopToBottom);
    thumbnails_->setWrapping(false);
    thumbnails_->setMovement(QListView::Static);
    thumbnails_->setIconSize(QSize(160, 200));
    thumbnails_->setGridSize(QSize(190, 230));
    thumbnails_->setSpacing(6);
    thumbnails_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    thumbnails_->viewport()->installEventFilter(this);
    thumbnailLayout->addWidget(thumbnails_);
    views_->addWidget(thumbnailCard);
    connect(thumbnails_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this] { refreshThumbnails(); });
    connect(toggle_, &QPushButton::clicked, this, [this](bool expanded) {
        expanded_ = expanded;
        updateExpandedState();
    });
    setEntries({});
}

void OutlinePanel::setEntries(const QVector<OutlineEntry> &entries)
{
    tree_->clear();
    addEntries(tree_, nullptr, entries);
    toggle_->setEnabled(!entries.isEmpty() || thumbnails_->count() > 0);
    updateExpandedState();
}

void OutlinePanel::setPages(int count, std::function<QImage(int)> render)
{
    render_ = std::move(render);
    thumbnails_->clear();
    for (int i = 0; i < count; ++i) {
        auto *item = new QListWidgetItem(tr("Page %1").arg(i + 1), thumbnails_);
        item->setData(Qt::UserRole, i);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setToolTip(tr("Go to page %1").arg(i + 1));
    }
    toggle_->setEnabled(count > 0 || tree_->topLevelItemCount() > 0);
    updateExpandedState();
    QTimer::singleShot(0, this, [this] { refreshThumbnails(); });
}

void OutlinePanel::setCurrentPage(int page)
{
    thumbnails_->setCurrentRow(page);
}

bool OutlinePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == thumbnails_->viewport() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, [this] { refreshThumbnails(); });
    return QFrame::eventFilter(watched, event);
}

void OutlinePanel::refreshThumbnails()
{
    if (!thumbnails_->isVisible() || !render_) return;
    for (int i = 0; i < thumbnails_->count(); ++i) {
        auto *item = thumbnails_->item(i);
        if (!item->icon().isNull() || item->data(Qt::UserRole + 1).toBool() ||
            !thumbnails_->visualItemRect(item).intersects(thumbnails_->viewport()->rect())) continue;
        item->setData(Qt::UserRole + 1, true);
        try {
            const QImage image = render_(i);
            const auto preview = QPixmap::fromImage(image).scaled(thumbnails_->iconSize(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
            // These are document previews, not UI glyphs. Prevent Qt from
            // tinting the selected/active image to match the application theme.
            QIcon icon;
            for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled})
                icon.addPixmap(preview, mode);
            item->setIcon(icon);
        } catch (const std::exception &e) {
            item->setToolTip(tr("Cannot render page %1: %2").arg(i + 1).arg(QString::fromUtf8(e.what())));
        }
    }
}

void OutlinePanel::updateExpandedState()
{
    const bool expanded = expanded_ && toggle_->isEnabled();
    toggle_->setChecked(expanded);
    toggle_->setText(expanded ? tr("›") : tr("‹"));
    toggle_->setToolTip(!toggle_->isEnabled() ? tr("Open a PDF to view outline and thumbnails")
                                            : expanded ? tr("Hide outline and thumbnails") : tr("Show outline and thumbnails"));
    toggle_->setAccessibleName(toggle_->toolTip());
    const bool hasOutline = tree_->topLevelItemCount() > 0;
    const bool outline = showOutline_ && hasOutline;
    outlineViewButton_->setEnabled(hasOutline);
    thumbnailViewButton_->setEnabled(thumbnails_->count() > 0);
    outlineViewButton_->setChecked(outline);
    thumbnailViewButton_->setChecked(!outline);
    views_->setCurrentIndex(outline ? 0 : 1);
    tree_->setVisible(expanded && outline);
    setVisible(expanded);
    if (expanded && !outline)
        QTimer::singleShot(0, this, [this] { refreshThumbnails(); });
}
