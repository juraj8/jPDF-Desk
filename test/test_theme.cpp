#include "main_window.h"
#include "ui/about_dialog.h"
#include "ui/editable_text.h"
#include "ui/dialog_helpers.h"
#include "ui/theme.h"
#include "ui/window_style.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>

namespace {
double luminance(const QColor &color)
{
    const auto linear = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}
double contrast(const char *first, const char *second)
{
    const double a = luminance(QColor(first)), b = luminance(QColor(second));
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    // Theme preferences in this test must never touch the real user's settings.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QCoreApplication::setOrganizationName(QStringLiteral("jPDF Desk tests"));
    QCoreApplication::setApplicationName(QStringLiteral("Theme"));
    for (const bool dark : {false, true}) {
        const auto &c = UiTheme::colors(dark);
        if (windowStyle(dark).contains(QLatin1Char('@'))) return 2;
        for (const auto background : {c.background, c.surface, c.hover, c.selection}) {
            if (contrast(c.text, background) < 4.5) return 3;
        }
        if (contrast(c.muted, c.surface) < 4.5 || contrast(c.onAccent, c.accent) < 4.5) return 4;
        if (UiTheme::palette(dark).color(QPalette::Text) != QColor(c.text)) return 5;
    }
    if (UiTheme::loadMode() != UiTheme::Mode::System) return 6;
    UiTheme::saveMode(UiTheme::Mode::Light);
    MainWindow window;
    window.show();
    app.processEvents();
    auto *view = window.findChild<QGraphicsView *>(QStringLiteral("documentView"));
    auto *dark = window.findChild<QAction *>(QStringLiteral("darkThemeAction"));
    auto *light = window.findChild<QAction *>(QStringLiteral("lightThemeAction"));
    auto *system = window.findChild<QAction *>(QStringLiteral("systemThemeAction"));
    if (!view || !dark || !light || !system || !light->isChecked()) return 7;
    dark->trigger();
    app.processEvents();
    if (!dark->isChecked() || light->isChecked() || UiTheme::loadMode() != UiTheme::Mode::Dark
        || view->backgroundBrush().color() != QColor(UiTheme::colors(true).workspace)
        || app.palette().color(QPalette::Window) != QColor(UiTheme::colors(true).background)) return 8;
    for (auto *item : view->scene()->items()) {
        auto *text = dynamic_cast<QGraphicsTextItem *>(item);
        if (!text) continue;
        const auto &c = UiTheme::colors(true);
        if (text->defaultTextColor() != QColor(text->data(1).toInt() == 1 ? c.text : c.muted)) return 9;
    }
    // Dialogs created after the switch must inherit the current appearance.
    AboutDialog about(window.windowIcon(), &window);
    if (about.palette().color(QPalette::WindowText) != QColor(UiTheme::colors(true).text)) return 10;
    auto *buttons = about.findChild<QDialogButtonBox *>();
    if (!buttons || !buttons->button(QDialogButtonBox::Close)) return 20;
    const auto closeIcon = buttons->button(QDialogButtonBox::Close)->icon().pixmap(16, 16).toImage();
    if (closeIcon.isNull() || closeIcon.pixelColor(7, 7) != QColor("#e85d67")) return 21;
    MainWindow nextSession;
    if (!nextSession.findChild<QAction *>(QStringLiteral("darkThemeAction"))->isChecked()) return 11;
    // Optional screenshots for manual review; no artifacts are written normally.
    const QString screenshots = qEnvironmentVariable("JPDF_DESK_THEME_SCREENSHOTS");
    if (!screenshots.isEmpty()) window.grab().save(screenshots + QStringLiteral("/dark.png"));

    const QString path = directory.filePath(QStringLiteral("input.pdf"));
    fz_context *context = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    pdf_document *document = pdf_create_document(context);
    pdf_obj *page = pdf_add_page(context, document, {0, 0, 600, 800}, 0, nullptr, nullptr);
    pdf_insert_page(context, document, -1, page);
    pdf_drop_obj(context, page);
    pdf_save_document(context, document, QFile::encodeName(path).constData(), &pdf_default_write_options);
    pdf_drop_document(context, document);
    fz_drop_context(context);
    window.openDocument(path);
    window.findChild<QPushButton *>(QStringLiteral("addButton"))->click();
    EditableText *annotation = nullptr;
    QGraphicsPixmapItem *paper = nullptr;
    for (auto *item : view->scene()->items()) {
        if (auto *text = dynamic_cast<EditableText *>(item)) annotation = text;
        if (auto *pixmap = dynamic_cast<QGraphicsPixmapItem *>(item)) paper = pixmap;
    }
    if (!annotation || !paper) return 12;
    for (const auto *name : {":/ui/up-light.xpm", ":/ui/down-light.xpm", ":/ui/up-dark.xpm", ":/ui/down-dark.xpm"})
        if (QPixmap(QString::fromLatin1(name)).isNull()) return 17;
    annotation->setPlainText(QStringLiteral("Unsaved text"));
    annotation->setPos(75, 125);
    const auto pixels = paper->pixmap().toImage();
    const auto scene = view->scene();
    const auto zoom = view->transform();
    light->trigger();
    app.processEvents();
    if (view->scene() != scene || annotation->toPlainText() != QStringLiteral("Unsaved text")
        || annotation->pos() != QPointF(75, 125) || !annotation->isSelected()
        || annotation->defaultTextColor() != QColor(Qt::black) || paper->pixmap().toImage() != pixels
        || view->transform() != zoom) return 13;
    if (!screenshots.isEmpty()) window.grab().save(screenshots + QStringLiteral("/light-document.png"));
    dark->trigger();
    app.processEvents();
    if (paper->pixmap().toImage() != pixels || !annotation->isSelected()) return 14;
    auto *thumbnails = window.findChild<QListWidget *>(QStringLiteral("pageThumbnails"));
    if (!thumbnails || !thumbnails->count()) return 18;
    const auto icon = thumbnails->item(0)->icon();
    if (icon.pixmap(thumbnails->iconSize(), QIcon::Normal).toImage()
        != icon.pixmap(thumbnails->iconSize(), QIcon::Selected).toImage()) return 19;
    if (!screenshots.isEmpty()) window.grab().save(screenshots + QStringLiteral("/dark-document.png"));
    system->trigger();
    if (UiTheme::loadMode() != UiTheme::Mode::System
        || view->backgroundBrush().color() != QColor(UiTheme::colors(UiTheme::isDark(UiTheme::Mode::System)).workspace)) return 15;
    QSettings().setValue(QStringLiteral("appearance/theme"), QStringLiteral("invalid"));
    if (UiTheme::loadMode() != UiTheme::Mode::System) return 16;
    return 0;
}
