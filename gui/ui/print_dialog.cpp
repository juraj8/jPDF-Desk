#include "ui/print_dialog.h"
#include "ui/print_preview.h"
#include "jpdf_desk/printing/print_settings.h"

#include <QCheckBox>
#include <QTabBar>
#include <QButtonGroup>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QTreeWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPrintEngine>
#include <QScrollArea>
#include <algorithm>
#include <QVBoxLayout>

PrintDialog::PrintDialog(QPrinter &printer, int pageCount, int currentPage, QWidget *parent,
                         PreviewRenderer renderPreview)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("printDialog"));
    setWindowTitle(tr("Print PDF"));
    resize(780, 600);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    auto *actions = new QHBoxLayout;
    auto *cancel = new QPushButton(tr("Cancel"), this);
    auto *preview = new QPushButton(tr("Preview"), this);
    preview->setObjectName(QStringLiteral("printPreviewButton"));
    preview->setEnabled(bool(renderPreview));
    auto *submit = new QPushButton(tr("Print"), this);
    submit->setObjectName(QStringLiteral("confirmPrint"));
    submit->setProperty("primary", true);
    actions->addWidget(cancel);
    actions->addStretch();
    actions->addWidget(preview);
    actions->addWidget(submit);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *switches = new QTabBar(this);
    switches->setObjectName(QStringLiteral("printViewCards"));
    switches->setAccessibleName(tr("Print settings sections"));
    switches->setDrawBase(false);
    switches->setExpanding(false);
    for (const auto &name : {tr("General"), tr("Page Setup"), tr("Job"), tr("Color"), tr("Advanced")})
        switches->addTab(name);
    layout->addWidget(switches);
    auto *views = new QStackedWidget(this);
    views->setObjectName(QStringLiteral("printViews"));
    layout->addWidget(views, 1);
    const auto view = [views] {
        auto *scroll = new QScrollArea(views);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *cards = new QWidget(scroll);
        auto *grid = new QGridLayout(cards);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(12);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        scroll->setWidget(cards);
        views->addWidget(scroll);
        return grid;
    };
    auto *documentCards = view();
    auto *pageCards = view();
    auto *jobCards = view();
    auto *colourCards = view();
    auto *advancedCards = view();
    connect(switches, &QTabBar::currentChanged, views, &QStackedWidget::setCurrentIndex);
    const auto card = [](QGridLayout *cardsLayout, const QString &title, int row, int column, int span = 1) {
        auto *frame = new QFrame(cardsLayout->parentWidget());
        frame->setObjectName(QStringLiteral("printCard"));
        auto *content = new QVBoxLayout(frame);
        content->setContentsMargins(9, 9, 9, 9);
        content->setSpacing(8);
        auto *heading = new QLabel(title, frame);
        heading->setObjectName(QStringLiteral("sectionHeading"));
        content->addWidget(heading);
        auto *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setVerticalSpacing(10);
        content->addLayout(form);
        content->addStretch();
        cardsLayout->addWidget(frame, row, column, 1, span);
        return form;
    };

    auto *destination = card(documentCards, tr("DESTINATION"), 0, 0, 2);
    auto *printers = new QTreeWidget(this);
    printers->setObjectName(QStringLiteral("printDestination"));
    printers->setHeaderLabels({tr("Name"), tr("Location"), tr("Status")});
    printers->setRootIsDecorated(false);
    printers->setMinimumHeight(180);
    printers->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    auto *file = new QTreeWidgetItem(printers, {tr("Print to File (PDF)"), QString(), tr("Ready")});
    file->setData(0, Qt::UserRole, QString());
    printers->setCurrentItem(file);
    for (const auto &name : QPrinterInfo::availablePrinterNames()) {
        const auto info = QPrinterInfo::printerInfo(name);
        const QString status = info.state() == QPrinter::Idle ? tr("Ready")
            : info.state() == QPrinter::Active ? tr("Printing")
            : info.state() == QPrinter::Error ? tr("Error") : tr("Unknown");
        auto *item = new QTreeWidgetItem(printers, {name, info.location(), status});
        item->setData(0, Qt::UserRole, name);
        if (info.isDefault()) printers->setCurrentItem(item);
    }
    const auto destinationName = [printers] { return printers->currentItem()->data(0, Qt::UserRole).toString(); };
    destination->addRow(printers);
    auto *output = new QLineEdit(this);
    output->setObjectName(QStringLiteral("printOutputPath"));
    output->setPlaceholderText(tr("Full path to a new PDF file"));
    auto *outputLabel = new QLabel(tr("Save to:"), this);
    destination->addRow(outputLabel, output);

    auto *pages = card(documentCards, tr("PAGES"), 1, 0);
    auto *range = new QButtonGroup(this);
    range->setObjectName(QStringLiteral("printRange"));
    const auto rangeOption = [this, range, pages](const QString &text, QPrinter::PrintRange id) {
        auto *option = new QRadioButton(text, this);
        range->addButton(option, id);
        pages->addRow(option);
        return option;
    };
    rangeOption(tr("All %1 pages").arg(pageCount), QPrinter::AllPages)->setChecked(true);
    rangeOption(tr("Current page (%1)").arg(currentPage + 1), QPrinter::CurrentPage);
    rangeOption(tr("Pages:"), QPrinter::PageRange);
    auto *pageRange = new QLineEdit(this);
    pageRange->setObjectName(QStringLiteral("printPageRange"));
    pageRange->setPlaceholderText(tr("For example: 1-3, 5, 8"));
    auto *rangeLabel = new QLabel(tr("Range:"), this);
    pages->addRow(rangeLabel, pageRange);
    auto *reverse = new QCheckBox(tr("Reverse page order"), this);

    auto *pageLayout = card(pageCards, tr("LAYOUT"), 0, 0);
    auto *duplex = new QComboBox(this);
    duplex->setObjectName(QStringLiteral("printDuplex"));
    pageLayout->addRow(tr("Two-sided:"), duplex);
    auto *perSheet = new QComboBox(this);
    perSheet->setObjectName(QStringLiteral("printPagesPerSheet"));
    for (int count : {1, 2, 4, 6, 9, 16}) perSheet->addItem(QString::number(count), count);
    pageLayout->addRow(tr("Pages per side:"), perSheet);
    auto *ordering = new QComboBox(this);
    ordering->addItem(tr("Left to right, top to bottom"), false);
    ordering->addItem(tr("Right to left, top to bottom"), true);
    pageLayout->addRow(tr("Page ordering:"), ordering);
    auto *subset = new QComboBox(this);
    subset->setObjectName(QStringLiteral("printSubset"));
    subset->addItem(tr("All pages"), int(PrintOptions::PageSubset::All));
    subset->addItem(tr("Odd pages only"), int(PrintOptions::PageSubset::Odd));
    subset->addItem(tr("Even pages only"), int(PrintOptions::PageSubset::Even));
    pageLayout->addRow(tr("Only print:"), subset);
    auto *scale = new QDoubleSpinBox(this);
    scale->setObjectName(QStringLiteral("printScale"));
    scale->setRange(10, 200);
    scale->setValue(100);
    scale->setSuffix(tr(" %"));
    pageLayout->addRow(tr("Scale:"), scale);

    // These backend-only options must not appear to work when Qt cannot apply them.
    const auto unavailable = [this](QFormLayout *form, const QString &label, const QString &value) {
        auto *option = new QComboBox(this);
        option->addItem(value);
        option->setEnabled(false);
        option->setToolTip(tr("This option is not exposed by the Qt printing backend."));
        form->addRow(label, option);
    };
    auto *settings = card(pageCards, tr("PAPER"), 0, 1);
    unavailable(settings, tr("Paper type:"), tr("Printer default"));
    auto *source = new QComboBox(this);
    source->setObjectName(QStringLiteral("printPaperSource"));
    settings->addRow(tr("Paper source:"), source);
    unavailable(settings, tr("Output tray:"), tr("Printer default"));
    auto *paper = new QComboBox(this);
    paper->setObjectName(QStringLiteral("printPaper"));
    settings->addRow(tr("Paper:"), paper);
    auto *orientation = new QComboBox(this);
    orientation->addItem(tr("Portrait"), QPageLayout::Portrait);
    orientation->addItem(tr("Landscape"), QPageLayout::Landscape);
    settings->addRow(tr("Orientation:"), orientation);
    auto *quality = card(colourCards, tr("COLOR"), 0, 0, 2);
    auto *colour = new QComboBox(this);
    colour->setObjectName(QStringLiteral("printColour"));
    quality->addRow(tr("Print Color Mode:"), colour);
    quality->addRow(tr("Printer Profile:"), new QLabel(tr("No profile available through Qt"), this));
    auto *resolution = new QComboBox(this);
    resolution->setObjectName(QStringLiteral("printResolution"));
    auto *advanced = card(advancedCards, tr("PRINT QUALITY"), 0, 0, 2);
    advanced->addRow(tr("Print Quality / Resolution:"), resolution);
    unavailable(advanced, tr("Print Optimization:"), tr("Printer default"));
    auto *scaling = new QComboBox(this);
    scaling->setObjectName(QStringLiteral("printScaling"));
    scaling->addItem(tr("Fit to printable area"), int(PrintOptions::Scaling::Fit));
    scaling->addItem(tr("Actual size (may crop)"), int(PrintOptions::Scaling::ActualSize));
    advanced->addRow(tr("Print Scaling:"), scaling);
    auto *qualityHint = new QLabel(tr("Document detail is rendered at up to 300 DPI."), this);
    qualityHint->setWordWrap(true);
    advanced->addRow(qualityHint);

    auto *job = card(jobCards, tr("JOB DETAILS"), 0, 0);
    auto *jobName = new QLineEdit(printer.docName(), this);
    jobName->setObjectName(QStringLiteral("printJobName"));
    job->addRow(tr("Job name:"), jobName);
    unavailable(job, tr("Priority:"), tr("Printer default"));
    auto *billing = new QLineEdit(this);
    billing->setEnabled(false);
    billing->setToolTip(tr("Billing information is not exposed by the Qt printing backend."));
    job->addRow(tr("Billing info:"), billing);
    auto *covers = card(jobCards, tr("ADD COVER PAGE"), 0, 1);
    unavailable(covers, tr("Before:"), tr("None"));
    unavailable(covers, tr("After:"), tr("None"));
    auto *schedule = card(jobCards, tr("PRINT DOCUMENT"), 1, 0, 2);
    auto *now = new QRadioButton(tr("Now"), this);
    now->setChecked(true);
    schedule->addRow(now);
    for (const auto &text : {tr("At a scheduled time (not supported)"), tr("On hold (not supported)")}) {
        auto *option = new QRadioButton(text, this);
        option->setEnabled(false);
        schedule->addRow(option);
    }
    auto *finishing = card(documentCards, tr("COPIES"), 1, 1);
    auto *copies = new QSpinBox(this);
    copies->setRange(1, 999);
    finishing->addRow(tr("Copies:"), copies);
    auto *collate = new QCheckBox(tr("Collate copies"), this);
    collate->setChecked(true);
    finishing->addRow(collate);
    finishing->addRow(reverse);
    documentCards->setRowStretch(2, 1);
    for (auto *grid : {pageCards, colourCards, advancedCards}) grid->setRowStretch(1, 1);
    jobCards->setRowStretch(2, 1);

    auto *error = new QLabel(this);
    error->setObjectName(QStringLiteral("printError"));
    error->setProperty("danger", true);
    error->setWordWrap(true);
    error->hide();
    layout->addWidget(error);
    layout->addLayout(actions);

    const auto updateDestination = [=, &printer] {
        const bool pdf = destinationName().isEmpty();
        output->setVisible(pdf);
        outputLabel->setVisible(pdf);
        source->clear();
        source->addItem(tr("Automatic"), QPrinter::Auto);
        if (!pdf) {
            QPrinter device(QPrinterInfo::printerInfo(destinationName()));
            const auto trays = device.printEngine()->property(QPrintEngine::PPK_PaperSources).toList();
            for (const auto &value : trays) {
                const auto tray = static_cast<QPrinter::PaperSource>(value.toInt());
                if (tray == QPrinter::Auto) continue;
                const QString label = tray == QPrinter::Manual ? tr("Manual feed")
                    : tray == QPrinter::Envelope ? tr("Envelope")
                    : tray == QPrinter::Cassette ? tr("Cassette")
                    : tray == QPrinter::LargeCapacity ? tr("Large capacity")
                    : tr("Tray %1").arg(int(tray));
                source->addItem(label, tray);
            }
        }
        source->setEnabled(source->count() > 1);
        const QPrinterInfo info = pdf ? QPrinterInfo() : QPrinterInfo::printerInfo(destinationName());
        paper->clear();
        auto sizes = info.supportedPageSizes();
        if (sizes.isEmpty()) sizes = {QPageSize(QPageSize::A4), QPageSize(QPageSize::Letter)};
        for (const auto &size : sizes)
            paper->addItem(size.name(), QVariant::fromValue(size));
        const QPageSize preferred = pdf ? printer.pageLayout().pageSize() : info.defaultPageSize();
        for (int i = 0; i < paper->count(); ++i)
            if (paper->itemData(i).value<QPageSize>().isEquivalentTo(preferred)) paper->setCurrentIndex(i);
        duplex->clear();
        duplex->addItem(tr("One-sided"), QPrinter::DuplexNone);
        const auto modes = info.supportedDuplexModes();
        if (modes.contains(QPrinter::DuplexLongSide)) duplex->addItem(tr("Two-sided (long edge)"), QPrinter::DuplexLongSide);
        if (modes.contains(QPrinter::DuplexShortSide)) duplex->addItem(tr("Two-sided (short edge)"), QPrinter::DuplexShortSide);
        duplex->setEnabled(duplex->count() > 1);
        colour->clear();
        const auto colourModes = info.supportedColorModes();
        // Missing capability information is not evidence of a monochrome device.
        if (pdf || colourModes.isEmpty() || colourModes.contains(QPrinter::Color))
            colour->addItem(tr("Colour"), QPrinter::Color);
        // Grayscale is also enforced on rendered images, even without driver support.
        colour->addItem(tr("Grayscale (black & white)"), QPrinter::GrayScale);
        const auto defaultColour = pdf ? printer.colorMode() : info.defaultColorMode();
        colour->setCurrentIndex(qMax(0, colour->findData(defaultColour)));
        colour->setEnabled(colour->count() > 1);
        resolution->clear();
        resolution->addItem(pdf ? tr("Standard (300 DPI)") : tr("Printer default"), pdf ? 300 : 0);
        auto resolutions = pdf ? QList<int>{72, 150, 300} : info.supportedResolutions();
        std::sort(resolutions.begin(), resolutions.end());
        for (int dpi : resolutions)
            if (dpi > 0 && resolution->findData(dpi) < 0)
                resolution->addItem(tr("%1 DPI").arg(dpi), dpi);
        resolution->setEnabled(resolution->count() > 1);
        error->hide();
    };
    connect(printers, &QTreeWidget::currentItemChanged, this, updateDestination);
    connect(range, &QButtonGroup::idToggled, this, [=] {
        const bool custom = range->checkedId() == QPrinter::PageRange;
        pageRange->setVisible(custom);
        rangeLabel->setVisible(custom);
        error->hide();
    });
    pageRange->hide();
    rangeLabel->hide();
    updateDestination();

    const auto showError = [error](const QString &message) {
        error->setText(message);
        error->setVisible(!message.isEmpty());
    };
    const auto readSettings = [=] {
        PrintSettings settings;
        settings.printerName = destinationName();
        settings.outputPath = output->text();
        settings.range = static_cast<QPrinter::PrintRange>(range->checkedId());
        settings.pageRange = pageRange->text();
        settings.options.pagesPerSheet = perSheet->currentData().toInt();
        settings.options.rightToLeft = ordering->currentData().toBool();
        settings.options.subset = static_cast<PrintOptions::PageSubset>(subset->currentData().toInt());
        settings.options.scaling = static_cast<PrintOptions::Scaling>(scaling->currentData().toInt());
        settings.options.scalePercent = scale->value();
        settings.resolution = resolution->currentData().toInt();
        settings.jobName = jobName->text();
        settings.paperSource = static_cast<QPrinter::PaperSource>(source->currentData().toInt());
        settings.colorMode = static_cast<QPrinter::ColorMode>(colour->currentData().toInt());
        settings.pageSize = paper->currentData().value<QPageSize>();
        settings.orientation = static_cast<QPageLayout::Orientation>(orientation->currentData().toInt());
        settings.duplex = static_cast<QPrinter::DuplexMode>(duplex->currentData().toInt());
        settings.copies = copies->value();
        settings.collate = collate->isChecked();
        settings.reverse = reverse->isChecked();
        return settings;
    };
    connect(submit, &QPushButton::clicked, this, [=, &printer] {
        const auto settings = readSettings();
        const QString message = settings.apply(printer, pageCount, currentPage, PrintSettings::Purpose::Print);
        showError(message);
        if (message.isEmpty()) {
            options_ = settings.options;
            accept();
        }
    });

    auto *previewView = new PrintPreview(renderPreview, showError, this);
    layout->insertWidget(layout->indexOf(views) + 1, previewView, 1);
    previewView->hide();
    connect(preview, &QPushButton::clicked, this, [=] {
        if (!previewView->isHidden()) {
            previewView->hide();
            switches->show();
            views->show();
            preview->setText(tr("Preview"));
            return;
        }
        const auto settings = readSettings();
        const QString message = previewView->configure(settings, pageCount, currentPage);
        showError(message);
        if (!message.isEmpty()) return;
        options_ = settings.options;
        switches->hide();
        views->hide();
        previewView->refresh();
        previewView->show();
        preview->setText(tr("Back to settings"));
    });
}
