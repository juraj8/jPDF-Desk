#include "main_window.h"
#include "document_view.h"
#include "document_loading.h"
#include "jpdf_desk/printing/pdf_printing.h"
#include "ui/about_dialog.h"
#include "ui/fileopen_dialog.h"
#include "ui/form_dialog.h"
#include "ui/metadata_dialog.h"
#include "ui/password_dialog.h"
#include "ui/page_controls.h"
#include "ui/search_controls.h"
#include "ui/print_dialog.h"
#include "ui/outline_panel.h"
#include "ui/sidebar.h"
#include "ui/signature_manager.h"
#include "ui/document_toolbar.h"
#include "ui/signature_check_dialog.h"
#ifdef JPDF_DESK_ONLINE_REVOCATION
#include "signature_revocation.h"
#endif
#include "ui/theme.h"
#include "ui/window_style.h"

#include <QActionGroup>
#include <QApplication>
#include <QStyleHints>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QMimeData>
#include <QUrl>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
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
    Q_INIT_RESOURCE(jpdf_desk_branding);
    Q_INIT_RESOURCE(jpdf_desk_ui);
    setWindowTitle(tr("jPDF Desk"));
    setWindowIcon(QIcon(QStringLiteral(":/branding/icon.png")));
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("centralWidget"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setAcceptDrops(true);
    welcome_ = new QWidget(root);
    welcome_->setObjectName(QStringLiteral("welcomePage"));
    auto *welcomeLayout = new QVBoxLayout(welcome_);
    welcomeLayout->setContentsMargins(24, 24, 24, 24);
    welcomeLayout->setSpacing(18);
    welcomeLayout->addStretch();
    auto *documentIcon = new QLabel(welcome_);
    documentIcon->setObjectName(QStringLiteral("welcomeIcon"));
    documentIcon->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(documentIcon);
    welcomeLayout->addSpacing(12);
    auto *welcomeHeading = new QLabel(tr("Open a Document"), welcome_);
    welcomeHeading->setObjectName(QStringLiteral("welcomeHeading"));
    welcomeHeading->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeHeading);
    auto *welcomeHint = new QLabel(tr("Drag and drop a PDF here"), welcome_);
    welcomeHint->setObjectName(QStringLiteral("welcomeHint"));
    welcomeHint->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeHint);
    welcomeLayout->addSpacing(16);
    auto *welcomeOpen = new QPushButton(tr("Open…"), welcome_);
    welcomeOpen->setObjectName(QStringLiteral("welcomeOpenButton"));
    welcomeOpen->setProperty("primary", true);
    welcomeOpen->setMinimumWidth(160);
    connect(welcomeOpen, &QPushButton::clicked, this, [this] { openFile(); });
    welcomeLayout->addWidget(welcomeOpen, 0, Qt::AlignHCenter);
    welcomeLayout->addStretch();
    rootLayout->addWidget(welcome_, 1);

    editor_ = new QWidget(root);
    auto *editorLayout = new QVBoxLayout(editor_);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    rootLayout->addWidget(editor_, 1);
    documentToolbar_ = new DocumentToolbar(editor_);
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    editorLayout->addLayout(layout, 1);

    sidebar_ = new Sidebar(windowIcon(), root);
    layout->addWidget(sidebar_);

    auto *workspace = new QWidget(root);
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    pageControls_ = new PageControls(workspace);
    auto *navigation = new QHBoxLayout;
    navigation->setContentsMargins(0, 0, 0, 0);
    navigation->setSpacing(0);
    navigation->addWidget(pageControls_, 1);
    navigation->addWidget(documentToolbar_);
    workspaceLayout->addLayout(navigation);
    view_ = new DocumentView(pdf_, workspace);
    workspaceLayout->addWidget(view_, 1);
    auto *search = pageControls_->searchControls();
    connect(search->queryControl(), &QLineEdit::textChanged, view_, &DocumentView::clearSearch);
    const auto findText = [this, search] { view_->findText(search->queryControl()->text()); };
    connect(search->queryControl(), &QLineEdit::returnPressed, this, findText);
    connect(search->previousButton(), &QPushButton::clicked, view_, [this] { view_->navigateMatch(-1); });
    connect(search->nextButton(), &QPushButton::clicked, view_, [this] { view_->navigateMatch(1); });
    connect(view_, &DocumentView::searchResultChanged, search, &SearchControls::setResultState);
    connect(view_, &DocumentView::searchError, this, [this](const QString &message) {
        QMessageBox::warning(this, tr("Cannot search PDF"), message);
    });
    connect(view_, &DocumentView::pdfError, this, [this](const QString &message) {
        QMessageBox::critical(this, tr("PDF error"), message);
    });
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [search] {
        if (search->queryControl()->isEnabled()) {
            search->queryControl()->setFocus();
            search->queryControl()->selectAll();
        }
    });
    auto *nextMatch = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(nextMatch, &QShortcut::activated, this, findText);
    auto *previousMatch = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3), this);
    connect(previousMatch, &QShortcut::activated, view_, [this] { view_->navigateMatch(-1); });
    layout->addWidget(workspace, 1);
    outlinePanel_ = new OutlinePanel(pageControls_->outlineToggleButton(), root);
    layout->addWidget(outlinePanel_);
    connect(pageControls_->outlineToggleButton(), &QPushButton::clicked, this, [layout] {
        layout->activate();
    });
    connect(outlinePanel_->tree(), &QTreeWidget::itemClicked, view_,
            [this](QTreeWidgetItem *item, int) { view_->goToPage(item->data(0, Qt::UserRole).toInt()); });
    connect(outlinePanel_->tree(), &QTreeWidget::itemActivated, view_,
            [this](QTreeWidgetItem *item, int) { view_->goToPage(item->data(0, Qt::UserRole).toInt()); });
    connect(outlinePanel_->thumbnails(), &QListWidget::itemClicked, view_,
            [this](QListWidgetItem *item) { view_->goToPage(item->data(Qt::UserRole).toInt()); });
    connect(outlinePanel_->thumbnails(), &QListWidget::itemActivated, view_,
            [this](QListWidgetItem *item) { view_->goToPage(item->data(Qt::UserRole).toInt()); });
    connect(view_, &DocumentView::currentPageChanged, this, [this](int page, int count) {
        pageControls_->setDocumentState(page, count);
        outlinePanel_->setCurrentPage(page);
    });
    auto *toggle = pageControls_->sidebarToggleButton();
    connect(toggle, &QPushButton::clicked, this, [toggle, layout, this](bool expanded) {
        sidebar_->setVisible(expanded);
        toggle->setText(expanded ? tr("‹") : tr("›"));
        toggle->setToolTip(expanded ? tr("Hide tools") : tr("Show tools"));
        toggle->setAccessibleName(toggle->toolTip());
        layout->activate();
    });

    setCentralWidget(root);
    for (auto *action : documentToolbar_->appearanceActions()->actions())
        action->setChecked(action->data().toString() == UiTheme::modeName(themeMode_));
    connect(documentToolbar_->appearanceActions(), &QActionGroup::triggered, this, [this](QAction *action) {
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
    connect(documentToolbar_->aboutAction(), &QAction::triggered, this, [this] {
        AboutDialog dialog(windowIcon(), this);
        dialog.exec();
    });
    auto *openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, [this] { openFile(); });
    connect(documentToolbar_->openAction(), &QAction::triggered, this, [this] { openFile(); });
    connect(documentToolbar_->saveAction(), &QAction::triggered, this, [this] { saveFile(); });
    connect(documentToolbar_->metadataAction(), &QAction::triggered, this, [this] { editMetadata(); });
    connect(sidebar_->formButton(), &QPushButton::clicked, this, [this] { editForm(); });
    connect(documentToolbar_->passwordAction(), &QAction::triggered, this, [this] { editPassword(); });
    connect(documentToolbar_->printAction(), &QAction::triggered, this, [this] { printFile(); });
    connect(sidebar_->signButton(), &QPushButton::clicked, this, [this] { saveFile(true); });
    connect(sidebar_->verifySignaturesButton(), &QPushButton::clicked, this, [this] { checkSignatures(); });
    connect(sidebar_->addButton(), &QPushButton::clicked, view_, &DocumentView::addText);
    connect(sidebar_->checkButton(), &QPushButton::clicked, view_,
            [this] { view_->addMark(OptionMark::Check); });
    connect(sidebar_->crossButton(), &QPushButton::clicked, view_,
            [this] { view_->addMark(OptionMark::Cross); });
    connect(sidebar_->loadSignatureButton(), &QPushButton::clicked, this,
            [this] { loadSignature(); });
    connect(documentToolbar_->manageSignatureImagesAction(), &QAction::triggered, this,
            [this] { loadSignature(); });
    connect(documentToolbar_->manageCertificatesAction(), &QAction::triggered, this,
            [this] { manageCertificates(); });
    connect(sidebar_->placeSignatureButton(), &QPushButton::clicked, view_,
            [this] {
                if (signatureTemplate_.isEmpty()) loadSignature();
                if (!signatureTemplate_.isEmpty()) view_->placeSignature(signatureTemplate_);
            });
    connect(pageControls_->previousButton(), &QPushButton::clicked, view_, [this] {
        view_->navigatePage(-1);
    });
    connect(pageControls_->nextButton(), &QPushButton::clicked, view_, [this] {
        view_->navigatePage(1);
    });
    refreshSignature();
    showDocument();
}

MainWindow::~MainWindow()
{
    // The view borrows pdf_; destroy it before the document member is released.
    delete view_;
}

void MainWindow::applyTheme()
{
    const bool dark = UiTheme::isDark(themeMode_);
    // Application-wide styling also reaches menus, message boxes and Qt dialogs.
    // Native OS file dialogs remain under the platform's own appearance.
    qApp->setPalette(UiTheme::palette(dark));
    qApp->setStyleSheet(windowStyle(dark));
    view_->setDarkTheme(dark);

    // Paint a neutral, scalable document outline in the current theme's muted color.
    const qreal ratio = devicePixelRatioF();
    QPixmap icon(QSize(144, 160) * ratio);
    icon.setDevicePixelRatio(ratio);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QString::fromLatin1(UiTheme::colors(dark).muted)), 8,
                        Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
    QPainterPath outline;
    outline.moveTo(94, 12);
    outline.lineTo(28, 12);
    outline.quadTo(12, 12, 12, 28);
    outline.lineTo(12, 132);
    outline.quadTo(12, 148, 28, 148);
    outline.lineTo(116, 148);
    outline.quadTo(132, 148, 132, 132);
    outline.lineTo(132, 50);
    outline.closeSubpath();
    painter.drawPath(outline);
    painter.drawLine(94, 12, 94, 50);
    painter.drawLine(94, 50, 132, 50);
    for (int y : {70, 88, 106}) painter.drawLine(38, y, 106, y);
    painter.drawLine(38, 124, 68, 124);
    painter.end();
    welcome_->findChild<QLabel *>(QStringLiteral("welcomeIcon"))->setPixmap(icon);
}

void MainWindow::showDocument(bool resetPage, const LoadedPdf *loaded)
{
    const int count = pdf_.pageCount();
    welcome_->setVisible(count == 0);
    editor_->setVisible(count > 0);
    documentToolbar_->setDocumentState(count > 0);
    sidebar_->setDocumentState(count, !signatureTemplate_.isEmpty(),
                               pdf_.canDigitallySign(), pdf_.canVerifySignatures());
    outlinePanel_->setPages(0, {});
    outlinePanel_->setEntries({});
    refreshThumbnails(loaded ? loaded->thumbnails : QVector<QImage>{});
    // Replace the old canvas before any outline error dialog runs an event loop.
    view_->showDocument(resetPage, loaded);
    if (loaded) {
        outlinePanel_->setEntries(loaded->outline);
        if (!loaded->outlineError.isEmpty())
            QMessageBox::warning(this, tr("Cannot load PDF outline"), loaded->outlineError);
    } else if (count) {
        try {
            outlinePanel_->setEntries(pdf_.outline());
        } catch (const std::exception &e) {
            // A malformed bookmark tree must not prevent viewing the PDF pages.
            QMessageBox::warning(this, tr("Cannot load PDF outline"), QString::fromUtf8(e.what()));
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile()
        && QFileInfo(urls.first().toLocalFile()).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0)
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.first().isLocalFile()
        || QFileInfo(urls.first().toLocalFile()).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0)
        return;
    event->acceptProposedAction();
    openDocument(urls.first().toLocalFile());
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF files (*.pdf)"));
    if (!path.isEmpty()) openDocument(path);
}

bool MainWindow::confirmUnsavedChanges()
{
    if (!documentModified_ && !view_->hasUnsavedChanges()) return true;
    QMessageBox prompt(QMessageBox::Warning, tr("Unsaved changes"),
        tr("Save your changes before closing this document?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    prompt.setObjectName(QStringLiteral("unsavedChangesDialog"));
    prompt.setDefaultButton(QMessageBox::Save);
    const int choice = prompt.exec();
    if (choice == QMessageBox::Save) return saveFile();
    return choice == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmUnsavedChanges()) event->accept();
    else event->ignore();
}

void MainWindow::openDocument(const QString &path)
{
    if (!confirmUnsavedChanges()) return;
    const bool firstDocument = pdf_.path().isEmpty();
    try {
        QString password;
        LoadedPdf loaded;
        bool retry = false;
        for (;;) {
            try {
                const qreal resolution = qMax(qreal(1), view_->transform().m11() * view_->devicePixelRatioF());
                const PdfSource source{path, password};
                if (!runPdfTask(this, tr("Opening PDF"), [&](const PdfTaskProgress &progress) {
                    loaded = loadPdf(source, resolution, progress);
                })) return;
                pdf_.swapContent(*loaded.document);
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
        if (signatureTemplate_.isEmpty()) signatureTemplate_ = loaded.signatureTemplate;
        sidebar_->setSignaturePreview(signatureTemplate_);
        showDocument(true, &loaded);
        documentModified_ = false;
        setWindowTitle(tr("jPDF Desk — %1").arg(QFileInfo(path).fileName()));
        if (firstDocument && !isFullScreen() && !isMaximized())
            setWindowState(windowState() | Qt::WindowMaximized);
    } catch (const std::exception &e) {
        const QString error = QString::fromUtf8(e.what());
        if (error.contains(QStringLiteral("unknown encryption handler: 'FOPN_foweb'")))
            showFileOpenWarning(this, path, error);
        else
            QMessageBox::critical(this, tr("Cannot open PDF"), error);
    }
}

void MainWindow::refreshThumbnails(const QVector<QImage> &previews)
{
    outlinePanel_->setPages(pdf_.pageCount(), [this, previews](int page) {
        if (page >= 0 && page < previews.size()) return previews[page];
        const QSizeF size = pdf_.pageSize(page);
        const int dpi = qMax(1, int(qMin(160.0 / size.width(), 200.0 / size.height()) * 72.0 * 1.5));
        return pdf_.renderForPrint(page, dpi);
    });
}

void MainWindow::editForm()
{
    if (!pdf_.pageCount()) return;
    try {
        const int page = view_->currentPage();
        FormDialog dialog(pdf_.formFields(page), page,
                          [this](const QMap<int, QString> &values) {
                              pdf_.setFormValues(values);
                              if (!values.isEmpty()) documentModified_ = true;
                          }, this);
        if (dialog.exec() != QDialog::Accepted) return;
        view_->refreshFormValues(); // Preserve annotation drafts and navigation.
        refreshThumbnails();
        outlinePanel_->setCurrentPage(page);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot edit PDF form"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::editPassword()
{
    if (!pdf_.pageCount()) return;
    PasswordDialog dialog([this](const QString &password) {
        pdf_.setPassword(password);
        documentModified_ = true;
    }, this);
    dialog.exec();
}

void MainWindow::editMetadata()
{
    if (!pdf_.pageCount()) return;
    try {
        MetadataDialog dialog(pdf_.metadata(), this);
        if (dialog.exec() != QDialog::Accepted) return;
        const auto changes = dialog.changes();
        pdf_.setMetadata(changes);
        if (!changes.isEmpty()) documentModified_ = true;
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot edit metadata"), QString::fromUtf8(e.what()));
    }
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

void MainWindow::printFile()
{
    if (!pdf_.pageCount()) return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QFileInfo(pdf_.path()).fileName());
    PrintDialog dialog(printer, pdf_.pageCount(), view_->currentPage(), this,
                       [this](QPrinter &previewPrinter, const PrintOptions &options) {
        printDocumentSnapshot(pdf_, view_->captureDrafts(), previewPrinter, view_->currentPage(), options);
    });
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        printDocumentSnapshot(pdf_, view_->captureDrafts(), printer, view_->currentPage(), dialog.options());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot print PDF"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::checkSignatures()
{
    if (!pdf_.pageCount()) return;
    try {
        const auto verify = [this] {
            QVector<DigitalSignatureStatus> checked;
            if (!runPdfTask(this, tr("Checking signatures"), [&](const PdfTaskProgress &progress) {
                checked = pdf_.checkDigitalSignatures();
                progress.checkCancelled();
            })) throw PdfOperationCancelled();
            return checked;
        };
        const auto signatures = verify();
        SignatureCheckDialog::OnlineCheck onlineCheck;
#ifdef JPDF_DESK_ONLINE_REVOCATION
        onlineCheck = [](QWidget *parent, QVector<DigitalSignatureStatus> &checked) {
            return runOnlineSignatureRevocation(parent, checked);
        };
#endif
        SignatureCheckDialog report(signatures, pdf_.signatureTrustDescription(), this, onlineCheck,
                                    verify);
        report.exec();
    } catch (const PdfOperationCancelled &) {
        return;
    } catch (const jpdf::crypto::ClockValidationError &e) {
        QMessageBox::warning(this, tr("System clock appears incorrect"),
            tr("Signature validity has not been determined. Correct the system date and time, then retry.\n\n%1")
                .arg(QString::fromUtf8(e.what())));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot check signatures"), QString::fromUtf8(e.what()));
    }
}

bool MainWindow::saveFile(bool digitallySign)
{
    if (!pdf_.pageCount()) return false;
    QString certificate, password;
    if (digitallySign) {
        if (QMessageBox::question(this, tr("Digital signature"),
            tr("Save and digitally sign the completed PDF, including all current edits and placed signature images? "
               "No intermediate save is needed. A signature image is optional. Further edits may invalidate the signature; "
               "existing digital signatures are not preserved by this save. "
               "Certificate trust depends on the recipient's PDF viewer.")) != QMessageBox::Yes) return false;
        // Always show the selected identity before asking for its password.
        SignatureManager manager(certificateStore_, this);
        if (manager.exec() != QDialog::Accepted) return false;
        try {
            certificate = certificateStore_.activePath();
        } catch (const std::exception &e) {
            QMessageBox::warning(this, tr("Cannot read certificate"), QString::fromUtf8(e.what()));
            return false;
        }
        if (certificate.isEmpty()) return false;
        bool ok = false;
        password = QInputDialog::getText(this, tr("Certificate password"), tr("Password:"),
                                         QLineEdit::Password, {}, &ok);
        if (!ok) return false;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Save PDF as"), {}, tr("PDF files (*.pdf)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".pdf");
    }
    try {
        const auto drafts = view_->captureDrafts();
        // Reusable signature images belong in the local library, not hidden
        // PDF metadata. Placed images are already included in the drafts.
        pdf_.saveSnapshot(path, drafts, {}, {certificate, password});
        showDocument();
        documentModified_ = false;
        setWindowTitle((digitallySign ? tr("jPDF Desk — Digitally signed %1")
                                     : tr("jPDF Desk — Saved %1")).arg(QFileInfo(path).fileName()));
        return true;
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot save PDF"), QString::fromUtf8(e.what()));
        return false;
    }
}
