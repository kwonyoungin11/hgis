#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace KaIcons {
QIcon icon(const QString& id);
QIcon icon(const QString& id, const QColor& ink);
QIcon appIcon();
}
