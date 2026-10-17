#pragma once
#include <QString>

// Interface language: English by default, Russian optional. Source strings are
// written in Russian; L() returns the English text when English is active.
void setLanguage(const QString &code);
QString currentLanguage();
QString L(const char *russian);
