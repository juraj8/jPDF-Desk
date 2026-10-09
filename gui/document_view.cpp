#include "document_view.h"
#include "document_loading.h"
#include "ui/editable_text.h"
#include "ui/mark_item.h"
#include "ui/page_annotations.h"
#include "ui/signature_item.h"
#include "ui/text_tools.h"
#include "ui/theme.h"
#include "ui/zoom_controls.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <exception>

DocumentView::DocumentView(PdfDocument &document, QWidget *parent)
    : QGraphicsView(parent), pdf_(document), scene_(new QGraphicsScene(this))
{
    setScene(scene_);
    setObjectName(QStringLiteral("documentView"));
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    textTools_ = new TextTools(this);
    zoomControls_ = new ZoomControls(this);
    for (auto *button : {zoomControls_->zoomInButton(), zoomControls_->zoomOutButton()})
        connect(button, &QPushButton::clicked, this, [this] {
            refreshPageImages();
            updateCurrentPage();
            updateTextTools();
        });
    connect(textTools_->removeButton(), &QPushButton::clicked, this, [this] {
        for (QGraphicsItem *item : scene_->selectedItems())
            if (isAnnotation(item)) delete item;
    });
    connect(scene_, &QGraphicsScene::selectionChanged, this, [this] {
        const auto selection = scene_->selectedItems();
        if (!selection.isEmpty()) {
            const bool text = selection.first()->type() == EditableText::Type;
            const bool signature = selection.first()->type() == SignatureItem::Type;
            if (isAnnotation(selection.first()))
                textTools_->setSelectionType(text, signature);
            if (signature) {
                QSignalBlocker blocker(textTools_->widthControl());
                textTools_->widthControl()->setValue(static_cast<SignatureItem *>(selection.first())->width());
            }
            if (text) {
                QSignalBlocker blocker(textTools_->sizeControl());
                textTools_->sizeControl()->setValue(qRound(static_cast<EditableText *>(selection.first())->fontSize()));
            }
        }
        updateTextTools();
    });
    connect(scene_, &QGraphicsScene::changed, this, [this] { updateTextTools(); });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        refreshPageImages();
        updateTextTools();
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateCurrentPage();
        refreshPageImages();
        updateTextTools();
    });
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] {
        updateCurrentPage();
        refreshPageImages();
    });
    connect(textTools_->sizeControl(), QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int points) {
        for (QGraphicsItem *item : scene_->selectedItems())
            if (auto *text = qgraphicsitem_cast<EditableText *>(item)) text->setFontSize(points);
    });
    connect(textTools_->widthControl(), QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int width) {
        for (QGraphicsItem *item : scene_->selectedItems())
            if (auto *signature = qgraphicsitem_cast<SignatureItem *>(item)) signature->setWidth(width);
        updateTextTools();
    });
}

DocumentView::~DocumentView()
{
    // Scene destruction can emit selection changes after overlays are deleted.
    disconnect(scene_, nullptr, this, nullptr);
    disconnect(horizontalScrollBar(), nullptr, this, nullptr);
    disconnect(verticalScrollBar(), nullptr, this, nullptr);
}

void DocumentView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateCurrentPage();
    refreshPageImages();
    updateTextTools();
}

void DocumentView::setDarkTheme(bool dark)
{
    dark_ = dark;
    const auto &colors = UiTheme::colors(dark);
    setBackgroundBrush(QColor(colors.workspace));
    // Recolor only empty-state copy; switching themes must preserve drafts.
    if (!pdf_.pageCount()) {
        for (auto *item : scene_->items()) {
            auto *text = dynamic_cast<QGraphicsTextItem *>(item);
            if (!text) continue;
            text->setDefaultTextColor(QColor(text->data(1).toInt() == 1 ? colors.text : colors.muted));
        }
    }
    viewport()->update();
}

void DocumentView::updateTextTools()
{
    const auto selection = scene_->selectedItems();
    if (selection.isEmpty() || !isAnnotation(selection.first())) {
        textTools_->hide();
        return;
    }
    textTools_->positionFor(selection.first());
}

DocumentAnnotations DocumentView::captureDrafts() const
{
    DocumentAnnotations snapshot;
    for (int i = 0; i < pages_.size(); ++i) {
        const auto annotations = captureAnnotations(*scene_, pages_[i].root);
        // Include empty pages so deleting the last annotation is saved too.
        snapshot.fields[i] = annotations.fields;
        snapshot.marks[i] = annotations.marks;
        snapshot.signatures[i] = annotations.signatures;
    }
    return snapshot;
}

void DocumentView::clearSearch()
{
    for (auto *highlight : searchHighlights_) delete highlight;
    searchHighlights_.clear();
    searchMatches_.clear();
    searchIndex_ = -1;
    emit searchResultChanged(-1, -1);
}

void DocumentView::findText(const QString &query)
{
    if (!pdf_.pageCount()) return;
    if (!searchMatches_.isEmpty()) {
        navigateMatch(1);
        return;
    }
    if (query.trimmed().isEmpty()) return;
    try {
        const PdfSource source = pdf_.savedSource();
        QVector<TextSearchMatch> matches;
        if (!runPdfTask(this, tr("Searching PDF"), [&](const PdfTaskProgress &progress) {
            PdfDocument reader;
            reader.open(source.path, source.password);
            matches = reader.search(query, [&](int completed, int total) {
                progress.report(completed, total);
                return true;
            });
        })) {
            clearSearch();
            return;
        }
        searchMatches_ = std::move(matches);
        emit searchResultChanged(-1, searchMatches_.size());
        if (!searchMatches_.isEmpty()) navigateMatch(1);
    } catch (const std::exception &e) {
        clearSearch();
        emit searchError(QString::fromUtf8(e.what()));
    }
}

void DocumentView::navigateMatch(int offset)
{
    if (searchMatches_.isEmpty()) return;
    const int count = searchMatches_.size();
    searchIndex_ = searchIndex_ < 0 ? (offset < 0 ? count - 1 : 0)
                                 : (searchIndex_ + offset + count) % count;
    for (auto *highlight : searchHighlights_) delete highlight;
    searchHighlights_.clear();
    const auto &match = searchMatches_[searchIndex_];
    if (match.page < 0 || match.page >= pages_.size()) return;
    QRectF bounds;
    for (const auto &rect : match.rects) {
        auto *highlight = new QGraphicsRectItem(rect, pages_[match.page].root);
        highlight->setPen(Qt::NoPen);
        highlight->setBrush(QColor(255, 200, 0, 100));
        highlight->setZValue(10);
        highlight->setAcceptedMouseButtons(Qt::NoButton);
        searchHighlights_.append(highlight);
        bounds = bounds.united(rect);
    }
    centerOn(pages_[match.page].root->mapToScene(bounds.center()));
    updateCurrentPage();
    refreshPageImages();
    updateTextTools();
    emit searchResultChanged(searchIndex_, count);
}

void DocumentView::navigatePage(int offset)
{
    goToPage(page_ + offset);
}

void DocumentView::goToPage(int target)
{
    if (target < 0 || target >= pages_.size()) return;
    const QRectF rect = pages_[target].root->sceneBoundingRect();
    const qreal halfHeight = viewport()->height() / (2 * transform().m22());
    centerOn(rect.center().x(), rect.top() + halfHeight);
    updateCurrentPage();
    refreshPageImages();
}

void DocumentView::updateCurrentPage()
{
    if (rebuilding_ || pages_.isEmpty()) return;
    const qreal center = mapToScene(viewport()->rect().center()).y();
    for (int i = 0; i < pages_.size(); ++i) {
        page_ = i;
        if (center <= pages_[i].root->sceneBoundingRect().bottom()) break;
    }
    emit currentPageChanged(page_, pages_.size());
}

QPointF DocumentView::insertionPoint() const
{
    const auto *root = pages_[page_].root;
    const QPointF center = root->mapFromScene(mapToScene(viewport()->rect().center()));
    const QRectF rect = root->rect();
    return QPointF(qBound(rect.left(), center.x(), rect.right()),
                   qBound(rect.top(), center.y(), rect.bottom()));
}

void DocumentView::refreshFormValues()
{
    clearSearch();
    for (auto &page : pages_) page.resolution = 0;
    refreshPageImages();
}

void DocumentView::refreshPageImages()
{
    if (rebuilding_ || pages_.isEmpty()) return;
    try {
        const qreal resolution = qMax(qreal(1), transform().m11() * devicePixelRatioF());
        const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
        for (int i = 0; i < pages_.size(); ++i) {
            auto &page = pages_[i];
            if (!page.root->sceneBoundingRect().intersects(visible)) {
                // Keep annotation items, but release off-screen raster memory.
                page.image->setPixmap(QPixmap());
                page.resolution = 0;
                continue;
            }
            if (qAbs(page.resolution - resolution) < 0.0001) continue;
            const QImage image = pdf_.render(i, resolution);
            page.image->setPixmap(QPixmap::fromImage(image));
            const QRectF rect = page.root->rect();
            page.image->setTransform(QTransform::fromScale(rect.width() / image.width(),
                                                          rect.height() / image.height()));
            page.resolution = resolution;
        }
    } catch (const std::exception &e) {
        emit pdfError(QString::fromUtf8(e.what()));
    }
}

void DocumentView::showDocument(bool resetPage, const LoadedPdf *loaded)
{
    clearSearch();
    rebuilding_ = true;
    if (resetPage) page_ = 0;
    pages_.clear();
    scene_->clear();
    const int count = pdf_.pageCount();
    zoomControls_->setDocumentAvailable(count > 0);
    if (!count) {
        page_ = 0;
        scene_->setSceneRect(0, 0, 900, 600);
        auto *heading = scene_->addText(tr("Your document starts here"));
        QFont headingFont = heading->font();
        headingFont.setPixelSize(24);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        heading->setData(1, 1);
        heading->setDefaultTextColor(QColor(UiTheme::colors(dark_).text));
        heading->setPos(450 - heading->boundingRect().width() / 2, 255);
        auto *caption = scene_->addText(tr("Open a PDF to add and edit text."));
        caption->setData(1, 2);
        caption->setDefaultTextColor(QColor(UiTheme::colors(dark_).muted));
        caption->setPos(450 - caption->boundingRect().width() / 2, 302);
        rebuilding_ = false;
        emit currentPageChanged(0, 0);
        return;
    }
    try {
        qreal y = 0, width = 0;
        for (int i = 0; i < count; ++i) {
            const QSizeF size = loaded ? loaded->pages[i].size : pdf_.pageSize(i);
            auto *root = scene_->addRect(QRectF(QPointF(), size), QPen(Qt::NoPen), QBrush(Qt::white));
            root->setPos(0, y);
            root->setData(0, i);
            auto *image = new QGraphicsPixmapItem(root);
            image->setTransformationMode(Qt::SmoothTransformation);
            qreal resolution = 0;
            if (loaded && i == 0 && !loaded->firstPage.isNull()) {
                image->setPixmap(QPixmap::fromImage(loaded->firstPage));
                image->setTransform(QTransform::fromScale(size.width() / loaded->firstPage.width(),
                                                          size.height() / loaded->firstPage.height()));
                resolution = loaded->resolution;
            }
            pages_.append({root, image, resolution});
            const auto annotations = loaded ? loaded->pages[i].annotations
                : PageAnnotations{pdf_.fields(i), pdf_.marks(i), pdf_.signatures(i)};
            addAnnotations(*scene_, annotations, root);
            width = qMax(width, size.width());
            y += size.height() + 24;
        }
        for (const auto &page : pages_)
            page.root->setX((width - page.root->rect().width()) / 2);
        scene_->setSceneRect(0, 0, width, y - 24);
    } catch (const std::exception &e) {
        emit pdfError(QString::fromUtf8(e.what()));
    }
    rebuilding_ = false;
    if (!pages_.isEmpty()) {
        page_ = qMin(page_, int(pages_.size()) - 1);
        const QRectF rect = pages_[page_].root->sceneBoundingRect();
        centerOn(rect.center().x(), rect.top() + viewport()->height() / (2 * transform().m22()));
    }
    updateCurrentPage();
    refreshPageImages();
}

void DocumentView::addText()
{
    if (pages_.isEmpty()) return;
    const QPointF center = insertionPoint();
    auto *item = new EditableText({QRectF(center, QSizeF(260, 45)), tr("Type here"),
                                   float(textTools_->sizeControl()->value())});
    item->setParentItem(pages_[page_].root);
    item->setTextInteractionFlags(Qt::TextEditorInteraction);
    item->setFocus();
    item->setSelected(true);
}

void DocumentView::addMark(OptionMark::Kind kind)
{
    if (pages_.isEmpty()) return;
    auto *item = new MarkItem({kind, insertionPoint()});
    item->setParentItem(pages_[page_].root);
    scene_->clearSelection();
    item->setSelected(true);
}

void DocumentView::placeSignature(const QByteArray &png)
{
    if (pages_.isEmpty() || png.isEmpty()) return;
    const QImage image = QImage::fromData(png, "PNG");
    if (image.isNull()) return;
    const qreal width = 180;
    const QPointF center = insertionPoint();
    auto *item = new SignatureItem({QRectF(center - QPointF(width / 2, width * image.height() / image.width() / 2),
                                           QSizeF(width, width * image.height() / image.width())), png});
    item->setParentItem(pages_[page_].root);
    scene_->clearSelection();
    item->setSelected(true);
}
