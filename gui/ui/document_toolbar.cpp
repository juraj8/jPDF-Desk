#include "ui/document_toolbar.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QPushButton>
#include <QPainter>

namespace {
class DocumentMenuButton : public QPushButton {
public:
    explicit DocumentMenuButton(QWidget *parent) : QPushButton(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPushButton::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                         QPalette::ButtonText));
        const QPointF center = QRectF(rect()).center();
        for (int offset : {-6, 0, 6})
            painter.drawEllipse(center + QPointF(0, offset), 1.5, 1.5);
    }
};
}

DocumentToolbar::DocumentToolbar(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("documentToolbar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 4, 8, 4);
    auto *more = new DocumentMenuButton(this);
    more->setObjectName(QStringLiteral("moreButton"));
    more->setFixedSize(36, 36);
    more->setToolTip(tr("Document actions"));
    more->setAccessibleName(more->toolTip());
    auto *menu = new QMenu(more);
    const auto action = [this, menu](const QString &text, const QString &name) {
        auto *result = menu->addAction(text);
        result->setObjectName(name);
        // Register menu shortcuts on the window even when the menu is closed.
        addAction(result);
        return result;
    };
    open_ = action(tr("Open…"), QStringLiteral("openButton"));
    menu->addSeparator();
    print_ = action(tr("Print…"), QStringLiteral("printButton"));
    print_->setShortcut(QKeySequence::Print);
    save_ = action(tr("Save As…"), QStringLiteral("saveButton"));
    save_->setShortcut(QKeySequence::Save);
    menu->addSeparator();
    metadata_ = action(tr("Document Properties…"), QStringLiteral("metadataButton"));
    metadata_->setToolTip(tr("View and edit PDF properties. Changes are included when you save."));
    password_ = action(tr("PDF Password…"), QStringLiteral("passwordButton"));
    password_->setToolTip(tr("Set, replace, or remove the password for the next saved copy."));
    menu->addSeparator();
    manageSignatureImages_ = action(tr("Manage Signature Images…"), QStringLiteral("manageSignatureImagesAction"));
    manageCertificates_ = action(tr("Manage Certificates…"), QStringLiteral("manageCertificatesAction"));
    manageCertificates_->setToolTip(tr("Import, choose, or manage certificates for digital signing."));
    menu->addSeparator();
    auto *appearance = menu->addMenu(tr("Appearance"));
    appearance->setObjectName(QStringLiteral("appearanceMenu"));
    appearanceActions_ = new QActionGroup(this);
    appearanceActions_->setExclusive(true);
    const auto addAppearance = [this, appearance](const QString &text, const QString &name) {
        auto *item = appearance->addAction(text);
        item->setObjectName(name + QStringLiteral("ThemeAction"));
        item->setData(name);
        item->setCheckable(true);
        appearanceActions_->addAction(item);
    };
    addAppearance(tr("System"), QStringLiteral("system"));
    addAppearance(tr("Light"), QStringLiteral("light"));
    addAppearance(tr("Dark"), QStringLiteral("dark"));
    about_ = action(tr("About…"), QStringLiteral("aboutButton"));
    more->setMenu(menu);
    layout->addWidget(more);
    setDocumentState(false);
}

void DocumentToolbar::setDocumentState(bool hasDocument)
{
    for (auto *action : {save_, print_, metadata_, password_})
        action->setEnabled(hasDocument);
}
