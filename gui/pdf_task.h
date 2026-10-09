#pragma once

#include <atomic>
#include <functional>
#include <QString>

class QWidget;

// Worker-to-GUI state only. No widgets, callbacks into a live PDF, or shared contexts.
class PdfTaskProgress {
public:
    void report(int completed, int total) const;
    void checkCancelled() const;
    void cancel() { cancelled_.store(true); }
    bool cancelled() const { return cancelled_.load(); }
    int completed() const { return completed_.load(); }
    int total() const { return total_.load(); }

private:
    mutable std::atomic<int> completed_{0};
    mutable std::atomic<int> total_{0};
    std::atomic<bool> cancelled_{false};
};

// Run on a joined worker with a modal, responsive progress/cancel dialog.
// Work owns its PDF/context. Returns false on cancellation, rethrows other errors.
// Call on the GUI thread. Captured results are safe to read only after return.
bool runPdfTask(QWidget *parent, const QString &title,
                const std::function<void(const PdfTaskProgress &)> &work);
