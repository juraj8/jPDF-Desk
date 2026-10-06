#include "ui/sidebar.h"

#include <QActionGroup>
#include <QGridLayout>
#include <QStyle>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidgetAction>

Sidebar::Sidebar(const QIcon &icon, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(248);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 24, 18, 20);
    layout->setSpacing(10);
    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    auto *logo = new QLabel(this);
    logo->setObjectName(QStringLiteral("brandLogo"));
    logo->setPixmap(icon.pixmap(44, 44));
    logo->setFixedSize(44, 44);
    brandRow->addWidget(logo);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto *title = new QLabel(tr("jPDF Desk"), this);
    title->setObjectName(QStringLiteral("windowHeading"));
    brandText->addWidget(title);
    auto *subtitle = new QLabel(tr("Because Acrobat sucks"), this);
    subtitle->setObjectName(QStringLiteral("windowSubtitle"));
    brandText->addWidget(subtitle);
    brandRow->addLayout(brandText);
    layout->addLayout(brandRow);
    layout->addSpacing(12);

    const auto button = [this](const QString &text, const QString &name) {
        auto *result = new QPushButton(text, this);
        result->setObjectName(name);
        return result;
    };
    open_ = button(tr("Open PDF"), QStringLiteral("openButton"));
    save_ = button(tr("Save as PDF"), QStringLiteral("saveButton"));
    save_->setProperty("primary", true);
    layout->addWidget(open_);
    layout->addWidget(save_);
    auto *fileRow = new QHBoxLayout;
    print_ = button(tr("Print…"), QStringLiteral("printButton"));
    print_->setToolTip(tr("Print all pages, a page range, or the current page, including unsaved edits."));
    fileRow->addWidget(print_);
    auto *more = button(tr("More…"), QStringLiteral("moreButton"));
    auto *menu = new QMenu(more);
    metadata_ = button(tr("File metadata…"), QStringLiteral("metadataButton"));
    metadata_->setToolTip(tr("View and edit PDF properties. Changes are included when you save."));
    auto *metadataAction = new QWidgetAction(menu);
    metadataAction->setDefaultWidget(metadata_);
    menu->addAction(metadataAction);
    connect(metadata_, &QPushButton::clicked, menu, &QMenu::hide);
    password_ = button(tr("PDF password…"), QStringLiteral("passwordButton"));
    password_->setToolTip(tr("Set, replace, or remove the password for the next saved copy."));
    auto *passwordAction = new QWidgetAction(menu);
    passwordAction->setDefaultWidget(password_);
    menu->addAction(passwordAction);
    connect(password_, &QPushButton::clicked, menu, &QMenu::hide);
    more->setMenu(menu);
    fileRow->addWidget(more);
    layout->addLayout(fileRow);
    layout->addSpacing(8);

    const auto heading = [this, layout](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("sectionHeading"));
        layout->addWidget(label);
    };
    heading(tr("TOOLS"));
    auto *tools = new QGridLayout;
    tools->setSpacing(8);
    const auto tile = [button, tools](const QString &symbol, const QString &label,
                                     const QString &name, int row, int column) {
        auto *result = button(symbol + QStringLiteral("\n") + label, name);
        result->setProperty("toolTile", true);
        result->setMinimumHeight(68);
        result->setAccessibleName(label);
        result->setToolTip(label);
        tools->addWidget(result, row, column);
        return result;
    };
    add_ = tile(QStringLiteral("T"), tr("Text"), QStringLiteral("addButton"), 0, 0);
    check_ = tile(QStringLiteral("✓"), tr("Checkmark"), QStringLiteral("checkButton"), 0, 1);
    cross_ = tile(QStringLiteral("×"), tr("Cross"), QStringLiteral("crossButton"), 1, 0);
    placeSignature_ = tile(QStringLiteral("✎"), tr("Signature"), QStringLiteral("placeSignatureButton"), 1, 1);
    layout->addLayout(tools);
    signaturePreview_ = new QLabel(this);
    signaturePreview_->setObjectName(QStringLiteral("sidebarSignaturePreview"));
    signaturePreview_->setAlignment(Qt::AlignCenter);
    signaturePreview_->setFixedHeight(42);
    layout->addWidget(signaturePreview_);
    loadSignature_ = button(tr("Manage signature images…"), QStringLiteral("loadSignatureButton"));
    loadSignature_->setProperty("subtle", true);
    layout->addWidget(loadSignature_);
    layout->addSpacing(8);
    heading(tr("DIGITAL SIGNATURES"));
    sign_ = button(tr("Sign PDF…"), QStringLiteral("signButton"));
    sign_->setToolTip(tr("Save with a cryptographic signature using a saved PKCS#12 certificate. Further edits may invalidate it."));
    layout->addWidget(sign_);
    verifySignatures_ = button(tr("Check signatures…"), QStringLiteral("verifySignaturesButton"));
    verifySignatures_->setToolTip(tr("Check signatures in the saved PDF, excluding unsaved edits."));
    layout->addWidget(verifySignatures_);
    manageCertificates_ = button(tr("Manage certificates…"), QStringLiteral("manageCertificatesButton"));
    manageCertificates_->setProperty("subtle", true);
    layout->addWidget(manageCertificates_);
    layout->addStretch();
    auto *appearance = button(tr("Appearance"), QStringLiteral("appearanceButton"));
    appearance->setProperty("subtle", true);
    appearance->setToolTip(tr("Choose System, Light, or Dark appearance. PDF colors stay unchanged."));
    auto *appearanceMenu = new QMenu(appearance);
    appearanceActions_ = new QActionGroup(this);
    appearanceActions_->setExclusive(true);
    const auto addAppearance = [&](const QString &text, const QString &name) {
        auto *action = appearanceMenu->addAction(text);
        action->setObjectName(name + QStringLiteral("ThemeAction"));
        action->setData(name);
        action->setCheckable(true);
        appearanceActions_->addAction(action);
    };
    addAppearance(tr("System"), QStringLiteral("system"));
    addAppearance(tr("Light"), QStringLiteral("light"));
    addAppearance(tr("Dark"), QStringLiteral("dark"));
    appearance->setMenu(appearanceMenu);
    layout->addWidget(appearance);
    about_ = button(tr("About…"), QStringLiteral("aboutButton"));
    about_->setProperty("subtle", true);
    layout->addWidget(about_);
    setSignaturePreview({});
}

void Sidebar::setSignaturePreview(const QByteArray &png)
{
    const auto image = QImage::fromData(png, "PNG");
    signaturePreview_->clear();
    signaturePreview_->setProperty("hasImage", !image.isNull());
    signaturePreview_->style()->unpolish(signaturePreview_);
    signaturePreview_->style()->polish(signaturePreview_);
    if (image.isNull()) signaturePreview_->setText(tr("No signature image selected"));
    else signaturePreview_->setPixmap(QPixmap::fromImage(image).scaled(200, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Sidebar::setDocumentState(int pageCount, bool hasSignature, bool canSign, bool canVerify)
{
    const bool hasDocument = pageCount > 0;
    for (auto *button : {add_, check_, cross_, save_, print_, metadata_, password_})
        button->setEnabled(hasDocument);
    sign_->setEnabled(hasDocument && canSign);
    verifySignatures_->setEnabled(hasDocument && canVerify);
    placeSignature_->setEnabled(hasDocument && hasSignature);
    placeSignature_->setToolTip(!hasSignature ? tr("Import or select an image in Manage signature images first.")
                                            : tr("Place the selected signature image on the PDF."));
}
