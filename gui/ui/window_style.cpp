#include "ui/window_style.h"
#include "ui/theme.h"

QString windowStyle(bool dark)
{
    const auto &c = UiTheme::colors(dark);
    QString style = QStringLiteral(R"(
        QMainWindow, QDialog, QWidget#centralWidget { background: @background; color: @text; }
        QLabel, QCheckBox, QRadioButton, QGroupBox { color: @text; }
        QFrame#sidebar { background: @surface; border-right: 1px solid @border; }
        QFrame#outlinePanel { background: @surface; border-left: 1px solid @border; }
        QFrame#outlineCard, QFrame#thumbnailCard, QFrame#printCard {
            background: @surface; border: 1px solid @border; border-radius: 7px;
        }
        QTreeWidget, QListWidget, QPlainTextEdit, QTextEdit, QLineEdit {
            background: @surface; color: @text; border: 1px solid @border;
            selection-background-color: @selection; selection-color: @text;
        }
        QTreeWidget#outlineTree, QListWidget#pageThumbnails { border: none; }
        QTreeWidget::item:hover, QListWidget::item:hover { background: @hover; }
        QTreeWidget::item:selected, QListWidget::item:selected {
            background: @selection; color: @text; border-radius: 5px;
        }
        QLineEdit, QPlainTextEdit, QTextEdit { padding: 5px; border-radius: 5px; }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
        QTreeWidget:focus, QListWidget:focus { border-color: @focus; }
        QLineEdit:disabled, QPlainTextEdit:disabled { color: @disabledText; background: @disabled; }
        QPushButton#previousButton, QPushButton#nextButton { padding: 6px 0; }
        QLabel#windowHeading { font-size: 22px; font-weight: 700; color: @text; }
        QLabel#windowSubtitle { color: @muted; font-size: 13px; }
        QLabel#sectionHeading { color: @muted; font-size: 11px; font-weight: 700; }
        QFrame#textTools, QFrame#zoomControls { background: @surface; border: 1px solid @strongBorder; }
        QFrame#textTools { border-radius: 10px; }
        QFrame#zoomControls { border-radius: 24px; }
        QPushButton {
            background: @surface; color: @text; border: 1px solid @border;
            border-radius: 7px; padding: 8px 12px; font-weight: 600;
        }
        QPushButton:hover { background: @hover; border-color: @strongBorder; }
        QPushButton:pressed { background: @pressed; }
        QPushButton:focus { border-color: @focus; }
        QPushButton#outlineViewButton:checked, QPushButton#thumbnailViewButton:checked {
            background: @selection; color: @text; border-color: @focus;
        }
        QPushButton:disabled { color: @disabledText; background: @disabled; border-color: @border; }
        QPushButton[primary="true"] { background: @accent; color: @onAccent; border-color: @accent; }
        QPushButton[primary="true"]:hover { background: @accentHover; border-color: @accentHover; }
        QPushButton[primary="true"]:pressed { background: @accentPressed; }
        QPushButton[primary="true"]:focus { border-color: @focus; }
        QPushButton[primary="true"]:disabled { background: @selection; border-color: @selection; color: @disabledText; }
        QPushButton[toolTile="true"] {
            background: @hover; border: 1px solid transparent; padding: 7px 4px; font-size: 14px;
        }
        QPushButton[toolTile="true"]:hover { background: @selection; border-color: @focus; }
        QPushButton[toolTile="true"]:focus { border-color: @focus; }
        QPushButton[toolTile="true"]:pressed { background: @pressed; }
        QPushButton[toolTile="true"]:disabled { background: @disabled; color: @disabledText; }
        QPushButton[subtle="true"] { border: 1px solid transparent; padding: 4px 1px; font-weight: 400; }
        QPushButton[subtle="true"]:hover { background: @hover; }
        QPushButton[subtle="true"]:focus { border-color: @focus; }
        QLabel#sidebarSignaturePreview { color: @muted; font-size: 11px; }
        QLabel#sidebarSignaturePreview[hasImage="true"], QLabel#signaturePreview {
            background: #ffffff; border: 1px solid @border; border-radius: 5px;
        }
        QFrame#zoomControls QPushButton { border-radius: 18px; padding: 0; font-size: 24px; }
        QPushButton#sidebarToggle, QPushButton#outlineToggle { padding: 6px 0; font-size: 18px; }
        QPushButton[danger="true"] { color: @danger; }
        QPushButton[danger="true"]:hover { background: @dangerHover; border-color: @danger; }
        QTabBar#printViewCards::tab {
            background: transparent; color: @muted; border: none;
            border-bottom: 3px solid @border; padding: 12px 18px;
            font-size: 14px;
        }
        QTabBar#printViewCards::tab:hover { background: @hover; color: @text; }
        QTabBar#printViewCards::tab:selected {
            color: @text; border-bottom: 3px solid @accent;
        }
        QTabBar#printViewCards::tab:focus { background: @hover; }
        QDialog#printDialog QFrame#printCard { background: transparent; border: none; }
        QDialog#printDialog QHeaderView::section {
            background: @surface; color: @muted; border: none; padding: 6px;
        }
        /* Print views share the outline/thumbnail card styling. */
        QDialog#printDialog QScrollArea, QDialog#printDialog QScrollArea > QWidget > QWidget {
            background: @background;
        }
        QLabel[danger="true"] { color: @danger; }
        QLabel#pageLabel { color: @text; font-weight: 600; }
        QSpinBox, QComboBox {
            background: @surface; color: @text; border: 1px solid @strongBorder;
            border-radius: 6px; padding: 5px 8px; min-width: 65px;
            selection-background-color: @selection; selection-color: @text;
        }
        QSpinBox { padding-right: 21px; }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border; width: 18px; background: @surface;
            border-left: 1px solid @border;
        }
        QSpinBox::up-button { subcontrol-position: top right; border-bottom: 1px solid @border; border-top-right-radius: 6px; }
        QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 6px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @hover; }
        QSpinBox::up-arrow { image: url(:/ui/up-@mode.xpm); width: 7px; height: 4px; }
        QSpinBox::down-arrow { image: url(:/ui/down-@mode.xpm); width: 7px; height: 4px; }
        QSpinBox:focus, QComboBox:focus { border-color: @focus; }
        QSpinBox:disabled, QComboBox:disabled { color: @disabledText; background: @disabled; }
        QMenu { background: @surface; color: @text; border: 1px solid @border; padding: 4px; }
        QMenu::item { padding: 7px 24px; border-radius: 4px; }
        QMenu::item:selected { background: @selection; }
        QMenu::item:disabled { color: @disabledText; }
        QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }
        QToolTip { background: @surface; color: @text; border: 1px solid @strongBorder; padding: 5px; }
        QScrollBar:vertical { background: @background; width: 12px; margin: 0; }
        QScrollBar:horizontal { background: @background; height: 12px; margin: 0; }
        QScrollBar::handle { background: @strongBorder; border: 3px solid @background; border-radius: 5px; }
        QScrollBar::handle:vertical { min-height: 24px; }
        QScrollBar::handle:horizontal { min-width: 24px; }
        QScrollBar::handle:hover { background: @muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QGraphicsView#documentView { border: 0; background: @workspace; }
    )");
    const auto replace = [&style](const char *name, const char *value) {
        style.replace(QString::fromLatin1(name), QString::fromLatin1(value));
    };
    replace("@mode", dark ? "dark" : "light");
    replace("@workspace", c.workspace);
    replace("@background", c.background);
    replace("@surface", c.surface);
    replace("@text", c.text);
    replace("@muted", c.muted);
    replace("@strongBorder", c.strongBorder);
    replace("@border", c.border);
    replace("@hover", c.hover);
    replace("@pressed", c.pressed);
    replace("@selection", c.selection);
    // Replace longer token names first, so @accent does not eat their prefixes.
    replace("@accentHover", c.accentHover);
    replace("@accentPressed", c.accentPressed);
    replace("@accent", c.accent);
    replace("@onAccent", c.onAccent);
    replace("@focus", c.focus);
    replace("@disabledText", c.disabledText);
    replace("@disabled", c.disabled);
    replace("@dangerHover", c.dangerHover);
    replace("@danger", c.danger);
    return style;
}
