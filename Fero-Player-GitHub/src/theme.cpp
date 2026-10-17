#include "theme.h"

const Palette &darkPalette() {
    static const Palette p{"#101216", "#191d25", "#191d25", "#242a35", "#2c3340", "#f5f6f8",
                           "#969faf", "#ffb15b", "#ffc27c", "#24180c", "#161a21", "#191e27",
                           "#424c5d", "#101216", "#1e232c", true};
    return p;
}

const Palette &lightPalette() {
    static const Palette p{"#f6f7fa", "#ffffff", "#ffffff", "#e7ebf2", "#dde2eb", "#202630",
                           "#717b8d", "#f6a345", "#ffb766", "#24180c", "#ffffff", "#ffffff",
                           "#d9dfe9", "#f6f7fa", "#e3e7ee", false};
    return p;
}

QString makeStyleSheet(const Palette &p) {
    QString check = p.dark ? QStringLiteral(":/check-dark.svg") : QStringLiteral(":/check-light.svg");
    QString css = QStringLiteral(R"CSS(
QWidget { color: @text; font-family: "Segoe UI", "Segoe UI Variable Text"; font-size: 13px; }
QMainWindow, QWidget#main, QWidget#content { background: @bg; }
QFrame#topbar { background: @topbar; border-bottom: 1px solid @subtle; }
QFrame#controls { background: @panel; border-top: 1px solid @border; }
QFrame#controls[floating="true"] { background: @floating; border: 1px solid @floatingBorder; border-radius: 18px; }
QLabel { background: transparent; }
QLabel#brand { font-size: 16px; font-weight: 600; }
QLabel#pageTitle { font-size: 24px; font-weight: 600; }
QLabel#muted, QLabel#time { color: @muted; }
QLabel#hint { color: @muted; font-size: 12px; }
QLabel#nowTitle { font-weight: 600; font-size: 14px; }
QPushButton, QToolButton { background: transparent; border: none; border-radius: 10px; padding: 8px 12px; }
QPushButton:hover, QToolButton:hover { background: @hover; }
QPushButton:pressed, QToolButton:pressed { background: @border; }
QPushButton:disabled, QToolButton:disabled { color: @muted; }
QPushButton#skipSegment { background: #1d1e24; color: #ffffff; border: 1px solid #5a5c66; border-radius: 10px; padding: 0 22px; min-height: 42px; font-size: 14px; font-weight: 600; }
QPushButton#skipSegment:hover { background: @accent; color: @accentText; border-color: @accent; }
QPushButton#primary { background: @accent; color: @accentText; font-weight: 600; padding: 0 16px; min-height: 36px; font-size: 13px; }
QPushButton#primary:hover { background: @accentHover; }
QPushButton#secondary { background: transparent; border: 1px solid @border; padding: 8px 12px; }
QToolButton#topIcon { background: @hover; border: none; border-radius: 10px; padding: 0; }
QToolButton#topIcon:hover { background: @border; }
QToolButton#topPrimary { background: @accent; border-radius: 10px; padding: 0; }
QToolButton#topPrimary:hover { background: @accentHover; }
QToolButton#iconButton { padding: 6px; border-radius: 10px; }
QToolButton#rateButton { padding: 4px 6px; border-radius: 10px; }
QLineEdit { background: @input; border: 1px solid @border; border-radius: 10px; padding: 9px 12px; selection-background-color: @accent; }
QLineEdit:focus { border: 1px solid @accent; }
QLineEdit#topSearch { background: @input; border: 1px solid @subtle; border-radius: 9px; padding: 0 10px; min-height: 34px; font-size: 13px; }
QLineEdit#topSearch:focus { border: 1px solid @accent; }
QComboBox, QSpinBox { background: @input; border: 1px solid @border; border-radius: 6px; padding: 8px 12px; }
QComboBox QAbstractItemView { background: @panel; selection-background-color: @hover; border: 1px solid @border; }
QListView { background: transparent; border: none; outline: none; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QMenu { background: @panel; border: 1px solid @border; border-radius: 12px; padding: 7px; }
QMenu::item { padding: 10px 26px 10px 14px; border-radius: 7px; }
QMenu::item:selected { background: @hover; }
QMenu::separator { height: 1px; background: @border; margin: 5px; }
QMenu::item:disabled { color: @muted; }
QMenu::indicator { width: 18px; height: 18px; }
QMenu::indicator:checked { image: url(@check); }
QMenu::indicator:unchecked { image: none; }
QDialog, QMessageBox { background: @bg; }
QCheckBox { spacing: 10px; padding: 6px 0; }
QCheckBox::indicator { width: 18px; height: 18px; background: @input; border: 1px solid @border; border-radius: 4px; }
QCheckBox::indicator:checked { background: @accent; border: 1px solid @accent; image: url(:/check-on-accent.svg); }
QToolTip { background: @panel; color: @text; border: 1px solid @border; padding: 6px; }
)CSS");
    const std::pair<const char *, QString> values[] = {
        {"@floatingBorder", p.floatingBorder}, {"@accentHover", p.accentHover},
        {"@accentText", p.accentText}, {"@floating", p.floating}, {"@topbar", p.topbar},
        {"@subtle", p.subtle}, {"@panel", p.panel}, {"@card", p.card}, {"@hover", p.hover},
        {"@border", p.border}, {"@text", p.text}, {"@muted", p.muted}, {"@accent", p.accent},
        {"@input", p.input}, {"@check", check}, {"@bg", p.bg}};
    for (const auto &[key, value] : values)
        css.replace(QLatin1String(key), value);
    return css;
}
