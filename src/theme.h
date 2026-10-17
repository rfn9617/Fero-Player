#pragma once
#include <QString>

struct Palette {
    QString bg, panel, card, hover, border, text, muted, accent, accentHover, accentText,
        input, floating, floatingBorder, topbar, subtle;
    bool dark = true;
};

const Palette &darkPalette();
const Palette &lightPalette();
QString makeStyleSheet(const Palette &p);
