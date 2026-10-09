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
#include "ui/theme.h"
#include "ui/window_style.h"

#include <QActionGroup>
#include <QApplication>
#include <QStyleHints>
#include <QDialog>
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
    connect(sidebar_->formButton(), &QPushButton::clicked, this, [this] { editForm(); });
    connect(sidebar_->passwordButton(), &QPushButton::clicked, this, [this] { editPassword(); });
    sidebar_->printButton()->setShortcut(QKeySequence::Print);
    connect(sidebar_->printButton(), &QPushButton::clicked, this, [this] { printFile(); });
    connect(sidebar_->signButton(), &QPushButton::clicked, this, [this] { saveFile(true); });
    connect(sidebar_->verifySignaturesButton(), &QPushButton::clicked, this, [this] { checkSignatures(); });
    connect(sidebar_->addButton(), &QPushButton::clicked, view_, &DocumentView::addText);
    connect(sidebar_->checkButton(), &QPushButton::clicked, view_,
            [this] { view_->addMark(OptionMark::Check); });
    connect(sidebar_->crossButton(), &QPushButton::clicked, view_,
            [this] { view_->addMark(OptionMark::Cross); });
    connect(sidebar_->loadSignatureButton(), &QPushButton::clicked, this,
            [this] { loadSignature(); });
    connect(sidebar_->manageCertificatesButton(), &QPushButton::clicked, this,
            [this] { manageCertificates(); });
    connect(sidebar_->placeSignatureButton(), &QPushButton::clicked, view_,
            [this] { view_->placeSignature(signatureTemplate_); });
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
}

void MainWindow::showDocument(bool resetPage, const LoadedPdf *loaded)
{
    const int count = pdf_.pageCount();
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

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF files (*.pdf)"));
    if (!path.isEmpty()) openDocument(path);
}

void MainWindow::openDocument(const QString &path)
{
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
        setWindowTitle(tr("jPDF Desk — %1").arg(QFileInfo(path).fileName()));
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
                          [this](const QMap<int, QString> &values) { pdf_.setFormValues(values); }, this);
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
        const auto drafts = view_->captureDrafts();
        pdf_.saveSnapshot(path, drafts, signatureTemplate_, {certificate, password});
        showDocument();
        setWindowTitle((digitallySign ? tr("jPDF Desk — Digitally signed %1")
                                     : tr("jPDF Desk — Saved %1")).arg(QFileInfo(path).fileName()));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Cannot save PDF"), QString::fromUtf8(e.what()));
    }
}
