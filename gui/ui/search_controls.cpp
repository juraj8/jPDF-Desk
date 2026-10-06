#include "ui/search_controls.h"
#include "ui/buttons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

SearchControls::SearchControls(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("searchControls"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    query_ = new QLineEdit(this);
    query_->setObjectName(QStringLiteral("searchQuery"));
    query_->setPlaceholderText(tr("Search PDF…"));
    query_->setAccessibleName(tr("Search PDF text"));
    query_->setToolTip(tr("Search selectable PDF text. Press Enter to search; scanned images require OCR."));
    query_->setClearButtonEnabled(true);
    query_->setMaximumWidth(240);
    layout->addWidget(query_);
    count_ = new QLabel(this);
    count_->setObjectName(QStringLiteral("searchResultCount"));
    layout->addWidget(count_);
    previous_ = navigationButton(QStringLiteral("↑"), QStringLiteral("previousMatch"), tr("Previous match (Shift+F3)"), this);
    next_ = navigationButton(QStringLiteral("↓"), QStringLiteral("nextMatch"), tr("Next match (F3)"), this);
    layout->addWidget(previous_);
    layout->addWidget(next_);
    setDocumentAvailable(false);
}

void SearchControls::setDocumentAvailable(bool available)
{
    query_->setEnabled(available);
    if (!available) setResultState(-1, -1);
}

void SearchControls::setResultState(int index, int count)
{
    count_->setText(index >= 0 ? tr("%1 / %2").arg(index + 1).arg(count)
                             : count == 0 && !query_->text().isEmpty() ? tr("No matches") : QString());
    previous_->setEnabled(count > 0);
    next_->setEnabled(count > 0);
}
