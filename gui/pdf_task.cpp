#include "pdf_task.h"
#include "jpdf_desk/document/pdf_document.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <exception>
#include <memory>

void PdfTaskProgress::checkCancelled() const
{
    if (cancelled()) throw PdfOperationCancelled();
}

void PdfTaskProgress::report(int completed, int total) const
{
    checkCancelled();
    total_.store(total);
    completed_.store(completed);
}

namespace {
class TaskDialog : public QDialog {
public:
    TaskDialog(QWidget *parent, const QString &title, PdfTaskProgress &progress)
        : QDialog(parent), progress_(progress)
    {
        setObjectName(QStringLiteral("pdfTaskDialog"));
        setWindowTitle(title);
        setWindowModality(Qt::ApplicationModal);
        setMinimumWidth(360);
        auto *layout = new QVBoxLayout(this);
        label_ = new QLabel(tr("Preparing document…"), this);
        label_->setTextFormat(Qt::PlainText);
        layout->addWidget(label_);
        bar_ = new QProgressBar(this);
        bar_->setObjectName(QStringLiteral("pdfTaskProgress"));
        bar_->setRange(0, 0);
        layout->addWidget(bar_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
        cancel_ = buttons->button(QDialogButtonBox::Cancel);
        cancel_->setObjectName(QStringLiteral("cancelPdfTask"));
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &TaskDialog::reject);
        connect(&timer_, &QTimer::timeout, this, [this] {
            const int total = progress_.total();
            const int completed = qMin(progress_.completed(), total);
            bar_->setRange(0, total);
            bar_->setValue(completed);
            if (!progress_.cancelled() && total > 0)
                label_->setText(tr("Processed %1 of %2 pages").arg(completed).arg(total));
        });
        timer_.start(50);
    }

    void reject() override
    {
        // Keep the modal barrier until the worker stops; never detach a worker
        // that still captures stack state or allow a reentrant document change.
        progress_.cancel();
        cancel_->setEnabled(false);
        label_->setText(tr("Cancelling… Waiting for the current PDF operation to finish."));
    }

private:
    PdfTaskProgress &progress_;
    QLabel *label_;
    QProgressBar *bar_;
    QPushButton *cancel_;
    QTimer timer_;
};
}

bool runPdfTask(QWidget *parent, const QString &title,
                const std::function<void(const PdfTaskProgress &)> &work)
{
    PdfTaskProgress progress;
    TaskDialog dialog(parent, title, progress);
    std::exception_ptr failure;
    auto thread = std::unique_ptr<QThread>(QThread::create([&] {
        try {
            progress.checkCancelled();
            work(progress);
        } catch (...) {
            failure = std::current_exception();
        }
    }));
    bool finished = false;
    QObject::connect(thread.get(), &QThread::finished, &dialog, [&] {
        finished = true;
        dialog.accept();
    });
    thread->start();
    dialog.exec();
    // Even application shutdown or an external dialog close must not leave a
    // worker using captured stack variables after this function returns.
    if (!finished) progress.cancel();
    thread->wait();
    if (progress.cancelled()) return false;
    if (failure) {
        try { std::rethrow_exception(failure); }
        catch (const PdfOperationCancelled &) { return false; }
    }
    return true;
}
