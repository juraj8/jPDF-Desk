#pragma once

#include <QPalette>
#include <QString>

// PDF annotation controls retain a consistent blue against the unchanged paper.
namespace UiColors {
inline constexpr char accent[] = "#2563eb";
}

namespace UiTheme {
enum class Mode { System, Light, Dark };
struct Colors {
    const char *workspace;
    const char *background;
    const char *surface;
    const char *text;
    const char *muted;
    const char *border;
    const char *strongBorder;
    const char *hover;
    const char *pressed;
    const char *selection;
    const char *accent;
    const char *accentHover;
    const char *accentPressed;
    const char *onAccent;
    const char *focus;
    const char *disabled;
    const char *disabledText;
    const char *danger;
    const char *dangerHover;
};
const Colors &colors(bool dark);
QPalette palette(bool dark);
bool isDark(Mode mode);
Mode loadMode();
void saveMode(Mode mode);
QString modeName(Mode mode);
Mode modeFromName(const QString &name);
}
