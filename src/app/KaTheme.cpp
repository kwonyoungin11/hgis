#include "KaTheme.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QWidget>

namespace KaTheme {
namespace {

const Tokens kTokens = {
    QColor(0xB8, 0xDF, 0xF6),  // sky0
    QColor(0x5E, 0xB3, 0xE4),  // sky1
    QColor(0x2E, 0x94, 0xD0),  // sky2
    QColor(0x18, 0x78, 0xB8),  // sky3
    QColor(0x0F, 0x5F, 0x9A),  // sky4
    QColor(0x0A, 0x4A, 0x7C),  // sky5
    QColor(0x06, 0x32, 0x5A),  // sky6
    QColor(0x0F, 0x17, 0x2A),  // ink
    QColor(0x33, 0x41, 0x55),  // inkMuted
    QColor(0x64, 0x74, 0x8B),  // inkDisabled
    QColor(0x00, 0x00, 0x00),  // border
    QColor(0xFF, 0xFF, 0xFF),  // bevelLight
    QColor(0x00, 0x00, 0x00),  // bevelDark
    QColor(0xE8, 0xEE, 0xF4),  // canvasNeutral
    QColor(0xE5, 0xE7, 0xEB),  // desk
    QColor(0xB9, 0x1C, 0x1C),  // danger
    QColor(0x16, 0x65, 0x34),  // ok
};

void setGroup(QPalette& pal, QPalette::ColorGroup g, const Tokens& t, bool disabled) {
  const QColor text = disabled ? t.inkDisabled : t.ink;
  pal.setColor(g, QPalette::Window, t.sky1);
  pal.setColor(g, QPalette::WindowText, text);
  pal.setColor(g, QPalette::Base, disabled ? t.sky0 : Qt::white);
  pal.setColor(g, QPalette::AlternateBase, disabled ? t.sky2 : t.sky0);
  pal.setColor(g, QPalette::Text, text);
  pal.setColor(g, QPalette::Button, disabled ? t.sky2 : t.sky1);
  pal.setColor(g, QPalette::ButtonText, text);
  pal.setColor(g, QPalette::BrightText, text);
  pal.setColor(g, QPalette::Highlight, disabled ? t.sky3 : t.sky5);
  pal.setColor(g, QPalette::HighlightedText, disabled ? t.ink : Qt::white);
  pal.setColor(g, QPalette::PlaceholderText, disabled ? QColor(0x94, 0xA3, 0xB8) : t.inkDisabled);
  pal.setColor(g, QPalette::ToolTipBase, t.sky0);
  pal.setColor(g, QPalette::ToolTipText, text);
  pal.setColor(g, QPalette::Light, disabled ? t.sky0 : t.bevelLight);
  pal.setColor(g, QPalette::Midlight, t.sky0);
  pal.setColor(g, QPalette::Mid, disabled ? t.sky2 : t.sky3);
  pal.setColor(g, QPalette::Dark, disabled ? t.inkDisabled : t.sky6);
  pal.setColor(g, QPalette::Shadow, disabled ? t.inkDisabled : t.border);
}

}  // namespace

const Tokens& tokens() { return kTokens; }

QPalette palette() {
  QPalette pal;
  setGroup(pal, QPalette::Active, kTokens, false);
  setGroup(pal, QPalette::Inactive, kTokens, false);
  setGroup(pal, QPalette::Disabled, kTokens, true);
  return pal;
}

QString embeddedStyleSheet() {
  return
#include "ka-hgis.qss.inc"
      ;
}

QStringList styleSheetCandidates() {
  const QString appDir = QCoreApplication::applicationDirPath();
  return {
      QDir(appDir).filePath(QStringLiteral("../data/theme/ka-hgis.qss")),
      QDir(appDir).filePath(QStringLiteral("data/theme/ka-hgis.qss")),
      QDir::current().filePath(QStringLiteral("data/theme/ka-hgis.qss")),
  };
}

QString resolveStyleSheetPath() {
  for (const QString& p : styleSheetCandidates()) {
    if (QFile::exists(p))
      return QFileInfo(p).absoluteFilePath();
  }
  return {};
}

QString loadStyleSheet() {
  const QStringList cands = styleSheetCandidates();
  for (const QString& p : cands) {
    qInfo() << "KaTheme QSS candidate:" << p;
    QFile f(p);
    if (!f.exists())
      continue;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "KaTheme QSS unreadable, skipping:" << p << f.errorString();
      continue;
    }
    const QString sheet = QString::fromUtf8(f.readAll());
    if (sheet.trimmed().isEmpty()) {
      qWarning() << "KaTheme QSS empty:" << p;
      continue;
    }
    qInfo() << "KaTheme QSS loaded from" << p;
    return sheet;
  }
  qInfo() << "KaTheme QSS using embedded fallback";
  return embeddedStyleSheet();
}

void apply(QApplication* app) {
  if (!app)
    return;
  app->setPalette(palette());
  app->setStyleSheet(loadStyleSheet());
}

void excludeMapSurface(QWidget* w) {
  if (!w)
    return;
  // Clears the *local* sheet only. Application QSS still applies; GIS exclude
  // selectors in ka-hgis.qss are the real protection.
  w->setStyleSheet(QString());
  w->setAttribute(Qt::WA_StyledBackground, false);
  if (auto* area = qobject_cast<QAbstractScrollArea*>(w)) {
    if (QWidget* vp = area->viewport()) {
      vp->setStyleSheet(QString());
      vp->setAttribute(Qt::WA_StyledBackground, false);
    }
  }
}

QString colorSwatchStyle(const QColor& fill) {
  const QColor use = fill.isValid() ? fill : QColor(Qt::white);
  return QStringLiteral("background-color: %1; border: 2px solid #000000; border-radius: 4px;")
      .arg(use.name());
}

}  // namespace KaTheme
