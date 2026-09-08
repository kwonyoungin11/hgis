#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace KaIcons {
// Group-colored field icons with explicit selection and disabled states.
QIcon icon(const QString& id);
// A valid ink requests a monochrome icon in every mode/state.
QIcon icon(const QString& id, const QColor& ink);
QIcon appIcon();
}
