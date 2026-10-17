#pragma once
#include <QIcon>
#include <QString>

// Original vector icons rendered at the actual size and device pixel ratio.
QIcon vectorIcon(const QString &name, const QString &color = QStringLiteral("#e8e8e8"));
QIcon applicationIcon();
