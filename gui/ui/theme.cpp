#include "ui/theme.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace UiTheme {
const Colors &colors(bool dark)
{
    static const Colors light = {
        "#e8edf4", "#f5f7fb", "#ffffff", "#334155", "#64748b", "#dce3ed", "#b9c9e2",
        "#f1f5fb", "#e4ebf5", "#dbe6f7", "#2563eb", "#1d4ed8", "#1e40af", "#ffffff",
        "#2563eb", "#f8fafc", "#8493a7", "#b42332", "#fff1f2"
    };
    static const Colors charcoal = {
        "#14181f", "#1b2029", "#232a35", "#e2e8f0", "#a4b0c2", "#364152", "#526179",
        "#2e3949", "#39485c", "#314a6c", "#2563eb", "#3070f0", "#1d4ed8", "#ffffff",
        "#8bb8ff", "#202630", "#748196", "#ff9ca9", "#442c38"
    };
    return dark ? charcoal : light;
}

QPalette palette(bool dark)
{
    const auto &c = colors(dark);
    QPalette result;
    result.setColor(QPalette::Window, QColor(c.background));
    result.setColor(QPalette::WindowText, QColor(c.text));
    result.setColor(QPalette::Base, QColor(c.surface));
    result.setColor(QPalette::AlternateBase, QColor(c.hover));
    result.setColor(QPalette::Text, QColor(c.text));
    result.setColor(QPalette::Button, QColor(c.surface));
    result.setColor(QPalette::ButtonText, QColor(c.text));
    result.setColor(QPalette::BrightText, QColor(c.onAccent));
    result.setColor(QPalette::Highlight, QColor(c.selection));
    result.setColor(QPalette::HighlightedText, QColor(c.text));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    result.setColor(QPalette::Accent, QColor(c.accent));
#endif
    result.setColor(QPalette::Link, QColor(c.focus));
    result.setColor(QPalette::LinkVisited, QColor(dark ? "#c4a8ff" : "#7145a0"));
    result.setColor(QPalette::ToolTipBase, QColor(c.surface));
    result.setColor(QPalette::ToolTipText, QColor(c.text));
    result.setColor(QPalette::PlaceholderText, QColor(c.muted));
    result.setColor(QPalette::Light, QColor(c.strongBorder));
    result.setColor(QPalette::Midlight, QColor(c.border));
    result.setColor(QPalette::Mid, QColor(c.border));
    result.setColor(QPalette::Dark, QColor(c.workspace));
    result.setColor(QPalette::Shadow, QColor(c.workspace));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::PlaceholderText})
        result.setColor(QPalette::Disabled, role, QColor(c.disabledText));
    for (const auto role : {QPalette::Base, QPalette::Button})
        result.setColor(QPalette::Disabled, role, QColor(c.disabled));
    return result;
}

bool isDark(Mode mode)
{
    if (mode != Mode::System) return mode == Mode::Dark;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    // Older Qt cannot reliably observe OS appearance; System falls back to Light.
    return false;
#endif
}
QString modeName(Mode mode)
{
    switch (mode) {
    case Mode::Light: return QStringLiteral("light");
    case Mode::Dark: return QStringLiteral("dark");
    default: return QStringLiteral("system");
    }
}
Mode modeFromName(const QString &name)
{
    if (name == QStringLiteral("light")) return Mode::Light;
    if (name == QStringLiteral("dark")) return Mode::Dark;
    return Mode::System;
}
Mode loadMode() { return modeFromName(QSettings().value(QStringLiteral("appearance/theme"), QStringLiteral("system")).toString()); }
void saveMode(Mode mode) { QSettings().setValue(QStringLiteral("appearance/theme"), modeName(mode)); }
}
