#include "ui/sidebar.h"

#include <QGridLayout>
#include <QStyle>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

Sidebar::Sidebar(const QIcon &icon, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(248);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 16);
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
    layout->addSpacing(8);

    const auto heading = [this, layout](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("sectionHeading"));
        layout->addWidget(label);
    };
    const auto button = [this](const QString &text, const QString &name) {
        auto *result = new QPushButton(text, this);
        result->setObjectName(name);
        return result;
    };
    heading(tr("FILL & ANNOTATE"));
    form_ = button(tr("Fill form fields…"), QStringLiteral("formButton"));
    form_->setToolTip(tr("Edit native text fields, checkboxes and choices on the current page."));
    layout->addWidget(form_);
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
    check_ = tile(QStringLiteral("✓"), tr("Check"), QStringLiteral("checkButton"), 0, 1);
    check_->setAccessibleName(tr("Checkmark"));
    check_->setToolTip(tr("Add a checkmark"));
    cross_ = tile(QStringLiteral("×"), tr("Cross"), QStringLiteral("crossButton"), 0, 2);
    for (int column = 0; column < 3; ++column)
        tools->setColumnStretch(column, 1);
    layout->addLayout(tools);
    layout->addSpacing(4);
    heading(tr("SIGN"));
    auto *signatureHint = new QLabel(tr("Place an image if needed, then digitally sign. No intermediate save required."), this);
    signatureHint->setObjectName(QStringLiteral("signatureWorkflowHint"));
    signatureHint->setTextFormat(Qt::PlainText);
    signatureHint->setWordWrap(true);
    layout->addWidget(signatureHint);
    auto *previewRow = new QHBoxLayout;
    signaturePreview_ = new QLabel(this);
    signaturePreview_->setObjectName(QStringLiteral("sidebarSignaturePreview"));
    signaturePreview_->setAlignment(Qt::AlignCenter);
    signaturePreview_->setFixedHeight(42);
    previewRow->addWidget(signaturePreview_, 1);
    loadSignature_ = button(tr("Change…"), QStringLiteral("loadSignatureButton"));
    loadSignature_->setProperty("subtle", true);
    previewRow->addWidget(loadSignature_);
    layout->addLayout(previewRow);
    placeSignature_ = button(tr("Place signature image"), QStringLiteral("placeSignatureButton"));
    layout->addWidget(placeSignature_);
    sign_ = button(tr("Sign digitally…"), QStringLiteral("signButton"));
    sign_->setToolTip(tr("Save and digitally sign all current edits, including placed signature images. "
                         "An image is optional and is not itself a digital signature. Further edits may invalidate signing."));
    layout->addWidget(sign_);
    verifySignatures_ = button(tr("Check signatures…"), QStringLiteral("verifySignaturesButton"));
    verifySignatures_->setToolTip(tr("Check digital signatures in the saved PDF, excluding unsaved edits."));
    layout->addWidget(verifySignatures_);
    layout->addStretch();
    setSignaturePreview({});
}

void Sidebar::setSignaturePreview(const QByteArray &png)
{
    const auto image = QImage::fromData(png, "PNG");
    signaturePreview_->clear();
    signaturePreview_->setProperty("hasImage", !image.isNull());
    signaturePreview_->style()->unpolish(signaturePreview_);
    signaturePreview_->style()->polish(signaturePreview_);
    signaturePreview_->setWordWrap(true);
    signaturePreview_->setToolTip(image.isNull() ? tr("Choose an image to place on the PDF. This is not a digital signature.")
                                               : tr("Selected signature image. Use Change to choose another."));
    if (image.isNull()) signaturePreview_->setText(tr("No image selected"));
    else signaturePreview_->setPixmap(QPixmap::fromImage(image).scaled(120, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Sidebar::setDocumentState(int pageCount, bool hasSignature, bool canSign, bool canVerify)
{
    const bool hasDocument = pageCount > 0;
    for (auto *button : {add_, check_, cross_, form_})
        button->setEnabled(hasDocument);
    sign_->setEnabled(hasDocument && canSign);
    verifySignatures_->setEnabled(hasDocument && canVerify);
    placeSignature_->setEnabled(hasDocument);
    placeSignature_->setToolTip(!hasSignature ? tr("Choose a signature image, then place and position it on the PDF.")
                                            : tr("Place and position the selected image, then optionally save and digitally sign."));
}
