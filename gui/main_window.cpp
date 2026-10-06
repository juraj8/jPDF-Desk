#include "main_window.h"
#include "pdf_filler/printing/pdf_printing.h"
#include "ui/about_dialog.h"
#include "ui/editable_text.h"
#include "ui/mark_item.h"
#include "ui/metadata_dialog.h"
#include "ui/password_dialog.h"
#include "ui/page_annotations.h"
#include "ui/page_controls.h"
#include "ui/search_controls.h"
#include "ui/print_dialog.h"
#include "ui/outline_panel.h"
#include "ui/sidebar.h"
#include "ui/signature_manager.h"
#include "ui/signature_item.h"
#include "ui/text_tools.h"
#include "ui/theme.h"
#include "ui/window_style.h"
#include "ui/zoom_controls.h"

#include <QActionGroup>
#include <QApplication>
#include <QGraphicsTextItem>
#include <QStyleHints>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QTreeWidget>
#include <QPrinter>

#include <QKeySequence>
#include <exception>
#include <utility>

MainWindow::MainWindow(QWidget *parent, SignatureServices services)
    : QMainWindow(parent), pdf_(std::move(services))
{
    Q_INIT_RESOURCE(pdf_filler_branding);
    Q_INIT_RESOURCE(pdf_filler_ui);
    setWindowTitle(tr("jPDF Desk"));
    setWindowIcon(QIcon(QStringLiteral(":/branding/icon.png")));
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("centralWidget"));
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new Sidebar(windowIcon(), root);
    layout->addWidget(sidebar_);

    auto *workspace = new QWidget(root);
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    pageControls_ = new PageControls(workspace);
    workspaceLayout->addWidget(pageControls_);
    auto *search = pageControls_->searchControls();
    connect(search->queryControl(), &QLineEdit::textChanged, this, [this] { clearSearch(); });
    connect(search->queryControl(), &QLineEdit::returnPressed, this, [this] { findText(); });
    connect(search->previousButton(), &QPushButton::clicked, this, [this] { navigateMatch(-1); });
    connect(search->nextButton(), &QPushButton::clicked, this, [this] { navigateMatch(1); });
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [search] {
        if (search->queryControl()->isEnabled()) {
            search->queryControl()->setFocus();
            search->queryControl()->selectAll();
        }
    });
    auto *nextMatch = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(nextMatch, &QShortcut::activated, this, [this] { findText(); });
    auto *previousMatch = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    connect(previousMatch, &QShortcut::activated, this, [this] { navigateMatch(-1); });
    layout->addWidget(workspace, 1);
    outlinePanel_ = new OutlinePanel(pageControls_->outlineToggleButton(), root);
    layout->addWidget(outlinePanel_);
    connect(pageControls_->outlineToggleButton(), &QPushButton::clicked, this, [this, layout] {
        layout->activate();
        updateTextTools();
    });
    connect(outlinePanel_->tree(), &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) { goToPage(item->data(0, Qt::UserRole).toInt()); });
    connect(outlinePanel_->tree(), &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item, int) { goToPage(item->data(0, Qt::UserRole).toInt()); });
    connect(outlinePanel_->thumbnails(), &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { goToPage(item->data(Qt::UserRole).toInt()); });
    connect(outlinePanel_->thumbnails(), &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { goToPage(item->data(Qt::UserRole).toInt()); });
    auto *toggle = pageControls_->sidebarToggleButton();
    connect(toggle, &QPushButton::clicked, this, [toggle, layout, this](bool expanded) {
        sidebar_->setVisible(expanded);
        toggle->setText(expanded ? tr("‹") : tr("›"));
        toggle->setToolTip(expanded ? tr("Hide tools") : tr("Show tools"));
        toggle->setAccessibleName(toggle->toolTip());
        layout->activate();
        updateTextTools();
    });

    scene_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(scene_, workspace);
    view_->setObjectName(QStringLiteral("documentView"));
    view_->setBackgroundBrush(QColor(UiTheme::colors(UiTheme::isDark(themeMode_)).workspace));
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setRenderHint(QPainter::SmoothPixmapTransform);
    workspaceLayout->addWidget(view_, 1);

    textTools_ = new TextTools(view_);
    zoomControls_ = new ZoomControls(view_);
    for (auto *button : {zoomControls_->zoomInButton(), zoomControls_->zoomOutButton()})
        connect(button, &QPushButton::clicked, this, [this] {
            refreshPageImages();
            updateCurrentPage();
            updateTextTools();
        });
    setCentralWidget(root);
    for (auto *action : sidebar_->appearanceActions()->actions())
        action->setChecked(action->data().toString() == UiTheme::modeName(themeMode_));
    connect(sidebar_->appearanceActions(), &QActionGroup::triggered, this, [this](QAction *action) {
        themeMode_ = UiTheme::modeFromName(action->data().toString());
        UiTheme::saveMode(themeMode_);
        applyTheme();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (themeMode_ == UiTheme::Mode::System) applyTheme();
    });
#endif
    applyTheme();
    resize(1100, 800);
    connect(sidebar_->aboutButton(), &QPushButton::clicked, this, [this] {
        AboutDialog dialog(windowIcon(), this);
        dialog.exec();
    });
    sidebar_->openButton()->setShortcut(QKeySequence::Open);
    sidebar_->saveButton()->setShortcut(QKeySequence::Save);
    connect(sidebar_->openButton(), &QPushButton::clicked, this, [this] { openFile(); });
    connect(sidebar_->saveButton(), &QPushButton::clicked, this, [this] { saveFile(); });
    connect(sidebar_->metadataButton(), &QPushButton::clicked, this, [this] { editMetadata(); });
    connect(sidebar_->passwordButton(), &QPushButton::clicked, this, [this] { editPassword(); });
    sidebar_->printButton()->setShortcut(QKeySequence::Print);
    connect(sidebar_->printButton(), &QPushButton::clicked, this, [this] { printFile(); });
    connect(sidebar_->signButton(), &QPushButton::clicked, this, [this] { saveFile(true); });
    connect(sidebar_->verifySignaturesButton(), &QPushButton::clicked, this, [this] { checkSignatures(); });
    connect(sidebar_->addButton(), &QPushButton::clicked, this, [this] { addText(); });
    connect(sidebar_->checkButton(), &QPushButton::clicked, this,
            [this] { addMark(OptionMark::Check); });
    connect(sidebar_->crossButton(), &QPushButton::clicked, this,
            [this] { addMark(OptionMark::Cross); });
    connect(sidebar_->loadSignatureButton(), &QPushButton::clicked, this,
            [this] { loadSignature(); });
    connect(sidebar_->manageCertificatesButton(), &QPushButton::clicked, this,
            [this] { manageCertificates(); });
    connect(sidebar_->placeSignatureButton(), &QPushButton::clicked, this,
            [this] { placeSignature(); });
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
    connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        refreshPageImages();
        updateTextTools();
    });
    connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateCurrentPage();
        refreshPageImages();
        updateTextTools();
    });
    connect(view_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] {
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
    connect(pageControls_->previousButton(), &QPushButton::clicked, this, [this] {
        navigatePage(-1);
    });
    connect(pageControls_->nextButton(), &QPushButton::clicked, this, [this] {
        navigatePage(1);
    });
    refreshSignature();
    showDocument();
}

void MainWindow::applyTheme()
{
    const bool dark = UiTheme::isDark(themeMode_);
    const auto &colors = UiTheme::colors(dark);
    // Application-wide styling also reaches menus, message boxes and Qt dialogs.
    // Native OS file dialogs remain under the platform's own appearance.
    qApp->setPalette(UiTheme::palette(dark));
    qApp->setStyleSheet(windowStyle(dark));
    view_->setBackgroundBrush(QColor(colors.workspace));
    // Recolor only the empty-state copy. Never rebuild the scene or change PDF
    // pixels/annotations when switching themes: unsaved edits must survive.
    if (!pdf_.pageCount()) {
        for (auto *item : scene_->items()) {
            auto *text = dynamic_cast<QGraphicsTextItem *>(item);
            if (!text) continue;
            text->setDefaultTextColor(QColor(text->data(1).toInt() == 1 ? colors.text : colors.muted));
        }
    }
    view_->viewport()->update();
}

MainWindow::~MainWindow()
{
    // The scene can emit selection changes while its children are destroyed,
    // after the viewport overlays have already been deleted.
    disconnect(scene_, nullptr, this, nullptr);
    disconnect(view_->horizontalScrollBar(), nullptr, this, nullptr);
    disconnect(view_->verticalScrollBar(), nullptr, this, nullptr);
}

void MainWindow::updateTextTools()
{
    const auto selection = scene_->selectedItems();
    if (selection.isEmpty() || !isAnnotation(selection.first())) {
        textTools_->hide();
        return;
    }
    textTools_->positionFor(selection.first());
}

DocumentAnnotations MainWindow::captureDrafts() const
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

void MainWindow::clearSearch()
{
    for (auto *highlight : searchHighlights_) delete highlight;
    searchHighlights_.clear();
    searchMatches_.clear();
    searchIndex_ = -1;
    pageControls_->searchControls()->setResultState(-1, -1);
}

void MainWindow::findText()
{
    if (!pdf_.pageCount()) return;
    if (!searchMatches_.isEmpty()) {
        navigateMatch(1);
        return;
    }
    const QString query = pageControls_->searchControls()->queryControl()->text();
    if (query.trimmed().isEmpty()) return;
    try {
        searchMatches_ = pdf_.search(query);
        pageControls_->searchControls()->setResultState(-1, searchMatches_.size());
        if (!searchMatches_.isEmpty()) navigateMatch(1);
    } catch (const std::exception &e) {
        clearSearch();
        QMessageBox::warning(this, tr("Cannot search PDF"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::navigateMatch(int offset)
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
    view_->centerOn(pages_[match.page].root->mapToScene(bounds.center()));
    updateCurrentPage();
    refreshPageImages();
    updateTextTools();
    pageControls_->searchControls()->setResultState(searchIndex_, count);
}

void MainWindow::navigatePage(int offset)
{
    goToPage(page_ + offset);
}

void MainWindow::goToPage(int target)
{
    if (target < 0 || target >= pages_.size()) return;
    const QRectF rect = pages_[target].root->sceneBoundingRect();
    const qreal halfHeight = view_->viewport()->height() / (2 * view_->transform().m22());
    view_->centerOn(rect.center().x(), rect.top() + halfHeight);
    updateCurrentPage();
    refreshPageImages();
}

void MainWindow::updateCurrentPage()
{
    if (rebuilding_ || pages_.isEmpty()) return;
    const qreal center = view_->mapToScene(view_->viewport()->rect().center()).y();
    for (int i = 0; i < pages_.size(); ++i) {
        page_ = i;
        if (center <= pages_[i].root->sceneBoundingRect().bottom()) break;
    }
    pageControls_->setDocumentState(page_, pages_.size());
    outlinePanel_->setCurrentPage(page_);
}

QPointF MainWindow::insertionPoint() const
{
    const auto *root = pages_[page_].root;
    const QPointF center = root->mapFromScene(view_->mapToScene(view_->viewport()->rect().center()));
    const QRectF rect = root->rect();
    return QPointF(qBound(rect.left(), center.x(), rect.right()),
                   qBound(rect.top(), center.y(), rect.bottom()));
}

void MainWindow::refreshPageImages()
{
    if (rebuilding_ || pages_.isEmpty()) return;
    try {
        const qreal resolution = qMax(qreal(1), view_->transform().m11() * view_->devicePixelRatioF());
        const QRectF visible = view_->mapToScene(view_->viewport()->rect()).boundingRect();
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
        QMessageBox::critical(this, tr("PDF error"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::showDocument()
{
    clearSearch();
    rebuilding_ = true;
    pages_.clear();
    scene_->clear();
    const int count = pdf_.pageCount();
    zoomControls_->setDocumentAvailable(count > 0);
    sidebar_->setDocumentState(count, !signatureTemplate_.isEmpty(),
                               pdf_.canDigitallySign(), pdf_.canVerifySignatures());
    pageControls_->setDocumentState(page_, count);
    outlinePanel_->setPages(0, {});
    outlinePanel_->setEntries({});
    if (!count) {
        scene_->setSceneRect(0, 0, 900, 600);
        auto *heading = scene_->addText(tr("Your document starts here"));
        QFont headingFont = heading->font();
        headingFont.setPixelSize(24);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        heading->setData(1, 1);
        heading->setDefaultTextColor(QColor(UiTheme::colors(UiTheme::isDark(themeMode_)).text));
        heading->setPos(450 - heading->boundingRect().width() / 2, 255);
        auto *caption = scene_->addText(tr("Open a PDF to add and edit text."));
        caption->setData(1, 2);
        caption->setDefaultTextColor(QColor(UiTheme::colors(UiTheme::isDark(themeMode_)).muted));
        caption->setPos(450 - caption->boundingRect().width() / 2, 302);
        rebuilding_ = false;
        return;
    }
    outlinePanel_->setPages(count, [this](int page) {
        const QSizeF size = pdf_.pageSize(page);
        // pageSize uses scene pixels at 1.5x; convert the preview scale to print DPI.
        const int dpi = qMax(1, int(qMin(160.0 / size.width(), 200.0 / size.height()) * 72.0 * 1.5));
        return pdf_.renderForPrint(page, dpi);
    });
    try {
        outlinePanel_->setEntries(pdf_.outline());
    } catch (const std::exception &e) {
        // A malformed bookmark tree must not prevent viewing the PDF pages.
        QMessageBox::warning(this, tr("Cannot load PDF outline"), QString::fromUtf8(e.what()));
    }
    try {
        qreal y = 0, width = 0;
        for (int i = 0; i < count; ++i) {
            const QSizeF size = pdf_.pageSize(i);
            auto *root = scene_->addRect(QRectF(QPointF(), size), QPen(Qt::NoPen), QBrush(Qt::white));
            root->setPos(0, y);
            root->setData(0, i);
            auto *image = new QGraphicsPixmapItem(root);
            image->setTransformationMode(Qt::SmoothTransformation);
            pages_.append({root, image, 0});
            addAnnotations(*scene_, {pdf_.fields(i), pdf_.marks(i), pdf_.signatures(i)}, root);
            width = qMax(width, size.width());
            y += size.height() + 24;
        }
        for (const auto &page : pages_)
            page.root->setX((width - page.root->rect().width()) / 2);
        scene_->setSceneRect(0, 0, width, y - 24);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("PDF error"), QString::fromUtf8(e.what()));
    }
    rebuilding_ = false;
    if (!pages_.isEmpty()) {
        page_ = qMin(page_, int(pages_.size()) - 1);
        const QRectF rect = pages_[page_].root->sceneBoundingRect();
        view_->centerOn(rect.center().x(), rect.top() + view_->viewport()->height() / (2 * view_->transform().m22()));
    }
    updateCurrentPage();
    refreshPageImages();
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF files (*.pdf)"));
    if (!path.isEmpty()) openDocument(path);
}

void MainWindow::openDocument(const QString &path)
{
    try {
        QString password;
        bool retry = false;
        for (;;) {
            try {
                pdf_.open(path, password);
                password.clear();
                break;
            } catch (const PdfPasswordRequired &) {
                bool accepted = false;
                password = QInputDialog::getText(this, tr("PDF password"),
                    retry ? tr("Incorrect password. Please try again:")
                          : tr("This PDF is password protected. Enter its password:"),
                    QLineEdit::Password, {}, &accepted);
                if (!accepted) return;
                retry = true;
            }
        }
        // Prefer the user's saved choice. A PDF's embedded template remains a
        // document-only fallback and is never silently imported into the library.
        refreshSignature();
        if (signatureTemplate_.isEmpty()) signatureTemplate_ = pdf_.signatureTemplate();
        sidebar_->setSignaturePreview(signatureTemplate_);
        page_ = 0;
        showDocument();
        setWindowTitle(tr("jPDF Desk — %1").arg(QFileInfo(path).fileName()));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot open PDF"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::editPassword()
{
    if (!pdf_.pageCount()) return;
    PasswordDialog dialog([this](const QString &password) { pdf_.setPassword(password); }, this);
    dialog.exec();
}

void MainWindow::editMetadata()
{
    if (!pdf_.pageCount()) return;
    try {
        MetadataDialog dialog(pdf_.metadata(), this);
        if (dialog.exec() != QDialog::Accepted) return;
        pdf_.setMetadata(dialog.changes());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot edit metadata"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::addText()
{
    if (!pdf_.pageCount()) return;
    if (pages_.isEmpty()) return;
    const QPointF center = insertionPoint();
    auto *item = new EditableText({QRectF(center, QSizeF(260, 45)), tr("Type here"),
                                   float(textTools_->sizeControl()->value())});
    item->setParentItem(pages_[page_].root);
    item->setTextInteractionFlags(Qt::TextEditorInteraction);
    item->setFocus();
    item->setSelected(true);
}

void MainWindow::addMark(OptionMark::Kind kind)
{
    if (!pdf_.pageCount()) return;
    if (pages_.isEmpty()) return;
    const QPointF center = insertionPoint();
    auto *item = new MarkItem({kind, center});
    item->setParentItem(pages_[page_].root);
    scene_->clearSelection();
    item->setSelected(true);
}

void MainWindow::refreshSignature()
{
    try {
        signatureTemplate_ = signatureStore_.activeImage();
        sidebar_->setSignaturePreview(signatureTemplate_);
        sidebar_->loadSignatureButton()->setToolTip(tr("Import, select, rename, or remove saved signature images."));
    } catch (const std::exception &e) {
        signatureTemplate_.clear();
        sidebar_->setSignaturePreview({});
        sidebar_->loadSignatureButton()->setToolTip(tr("Cannot read saved signature: %1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::loadSignature()
{
    SignatureManager dialog(signatureStore_, this);
    dialog.exec();
    refreshSignature();
    sidebar_->setDocumentState(pdf_.pageCount(), !signatureTemplate_.isEmpty(),
                               pdf_.canDigitallySign(), pdf_.canVerifySignatures());
}

void MainWindow::manageCertificates()
{
    SignatureManager dialog(certificateStore_, this);
    dialog.exec();
}

void MainWindow::placeSignature()
{
    if (!pdf_.pageCount() || signatureTemplate_.isEmpty()) return;
    const QImage image = QImage::fromData(signatureTemplate_, "PNG");
    if (image.isNull()) return;
    const qreal width = 180;
    if (pages_.isEmpty()) return;
    const QPointF center = insertionPoint();
    auto *item = new SignatureItem({QRectF(center - QPointF(width / 2, width * image.height() / image.width() / 2),
                                           QSizeF(width, width * image.height() / image.width())), signatureTemplate_});
    item->setParentItem(pages_[page_].root);
    scene_->clearSelection();
    item->setSelected(true);
}

void MainWindow::printFile()
{
    if (!pdf_.pageCount()) return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QFileInfo(pdf_.path()).fileName());
    PrintDialog dialog(printer, pdf_.pageCount(), page_, this,
                       [this](QPrinter &previewPrinter, const PrintOptions &options) {
        printDocumentSnapshot(pdf_, captureDrafts(), previewPrinter, page_, options);
    });
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        printDocumentSnapshot(pdf_, captureDrafts(), printer, page_, dialog.options());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot print PDF"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::checkSignatures()
{
    if (!pdf_.pageCount()) return;
    try {
        const auto signatures = pdf_.checkDigitalSignatures();
        QStringList details;
        for (int i = 0; i < signatures.size(); ++i) {
            const auto &signature = signatures[i];
            QString text = tr("Signature %1 — %2").arg(i + 1).arg(signature.fieldName);
            if (!signature.error.isEmpty()) {
                text += tr("\nCould not verify: %1").arg(signature.error);
            } else if (!signature.signedField) {
                text += tr("\nUnsigned signature field.");
            } else {
                text += tr("\nSigner: %1").arg(signature.signer.isEmpty() ? tr("Unknown") : signature.signer);
                text += signature.digestValid ? tr("\nIntegrity: signed bytes are intact.")
                    : tr("\nIntegrity check FAILED: %1").arg(signature.digestStatus);
                text += signature.certificateTrusted
                    ? tr("\nCertificate: accepted by the verification provider's trust policy.")
                    : tr("\nCertificate not trusted or validation failed: %1").arg(signature.certificateStatus);
                text += signature.changedSinceSigning
                    ? tr("\nLater PDF revisions exist: the signature does not cover the current document in full.")
                    : tr("\nNo later PDF revisions detected.");
            }
            details.append(text);
        }
        QMessageBox report(this);
        report.setWindowTitle(tr("Digital signature check"));
        report.setIcon(QMessageBox::Information);
        report.setTextFormat(Qt::PlainText);
        report.setText(signatures.isEmpty() ? tr("No digital signature fields found.")
            : tr("Found %1 digital signature field(s). Open Details for the results.").arg(signatures.size()));
        report.setInformativeText(tr("Checks the saved PDF only; unsaved edits are excluded. "
            "Integrity and certificate trust are separate checks. Trust policy: %1")
            .arg(pdf_.signatureTrustDescription()));
        if (!details.isEmpty()) report.setDetailedText(details.join(QStringLiteral("\n\n")));
        report.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot check signatures"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::saveFile(bool digitallySign)
{
    if (!pdf_.pageCount()) return;
    QString certificate, password;
    if (digitallySign) {
        if (QMessageBox::question(this, tr("Digital signature"),
            tr("Sign the completed PDF with your certificate? Further edits may invalidate the signature; "
               "existing digital signatures are not preserved by this save. "
               "Certificate trust depends on the recipient's PDF viewer.")) != QMessageBox::Yes) return;
        // Always show the selected identity before asking for its password.
        SignatureManager manager(certificateStore_, this);
        if (manager.exec() != QDialog::Accepted) return;
        try {
            certificate = certificateStore_.activePath();
        } catch (const std::exception &e) {
            QMessageBox::warning(this, tr("Cannot read certificate"), QString::fromUtf8(e.what()));
            return;
        }
        if (certificate.isEmpty()) return;
        bool ok = false;
        password = QInputDialog::getText(this, tr("Certificate password"), tr("Password:"),
                                         QLineEdit::Password, {}, &ok);
        if (!ok) return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save PDF as"), {}, tr("PDF files (*.pdf)"));
    if (path.isEmpty()) return;
    try {
        const auto drafts = captureDrafts();
        pdf_.saveSnapshot(path, drafts, signatureTemplate_, {certificate, password});
        showDocument();
        setWindowTitle((digitallySign ? tr("jPDF Desk — Digitally signed %1")
                                     : tr("jPDF Desk — Saved %1")).arg(QFileInfo(path).fileName()));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot save PDF"), QString::fromUtf8(e.what()));
    }
}
