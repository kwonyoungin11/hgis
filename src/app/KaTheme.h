#pragma once
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStringList>

class QApplication;
class QWidget;

namespace KaTheme {

struct Tokens {
  QColor sky0, sky1, sky2, sky3, sky4, sky5, sky6;
  QColor ink, inkMuted, inkDisabled, border, bevelLight, bevelDark;
  QColor canvasNeutral, desk, danger, ok;
};

const Tokens& tokens();

QPalette palette();
QString embeddedStyleSheet();
QStringList styleSheetCandidates();
QString resolveStyleSheetPath();
QString loadStyleSheet();
void apply(QApplication* app);
void excludeMapSurface(QWidget* w);
QString colorSwatchStyle(const QColor& fill);

}  // namespace KaTheme
