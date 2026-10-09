#pragma once

#include "jpdf_desk/printing/print_settings.h"
#include <QWidget>
#include <functional>

class QPrintPreviewWidget;
class QSpinBox;

// Owns an independent printer, rendering, page navigation and display-only zoom.
class PrintPreview : public QWidget {
public:
    using Renderer = std::function<void(QPrinter &, const PrintOptions &)>;
    using ErrorHandler = std::function<void(const QString &)>;

    PrintPreview(Renderer renderer, ErrorHandler onError, QWidget *parent = nullptr);
    ~PrintPreview() override;
    QString configure(const PrintSettings &settings, int pageCount, int currentPage);
    void refresh();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setZoom(qreal factor);
    QPrinter printer_{QPrinter::HighResolution};
    PrintOptions options_;
    QPrintPreviewWidget *view_;
    QSpinBox *zoom_;
};
