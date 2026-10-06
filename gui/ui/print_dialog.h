#pragma once

#include <QDialog>
#include "pdf_filler/printing/pdf_printing.h"
#include <functional>

class QPrinter;

// Configures the supplied printer only when the user confirms the dialog.
class PrintDialog : public QDialog {
public:
    using PreviewRenderer = std::function<void(QPrinter &, const PrintOptions &)>;
    PrintDialog(QPrinter &printer, int pageCount, int currentPage, QWidget *parent = nullptr,
                PreviewRenderer renderPreview = {});
    const PrintOptions &options() const { return options_; }

private:
    PrintOptions options_;
};
