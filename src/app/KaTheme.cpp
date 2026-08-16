#include "KaTheme.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>
#include <QWidget>

namespace KaTheme {
namespace {

const Tokens kTokens = {
    QColor(0xEF, 0xF6, 0xFF),  // sky0 primary pale
    QColor(0x25, 0x63, 0xEB),  // sky1 primary
    QColor(0x1D, 0x4E, 0xD8),  // sky2
    QColor(0x1E, 0x40, 0xAF),  // sky3
    QColor(0xF8, 0xFA, 0xFC),  // sky4 surface
    QColor(0x25, 0x63, 0xEB),  // sky5
    QColor(0x1E, 0x29, 0x3B),  // sky6 ink
    QColor(0x1E, 0x29, 0x3B),  // ink
    QColor(0x64, 0x74, 0x8B),  // inkMuted
    QColor(0x94, 0xA3, 0xB8),  // inkDisabled
    QColor(0xE5, 0xE7, 0xEB),  // border
    QColor(0xFF, 0xFF, 0xFF),  // bevelLight
    QColor(0xCB, 0xD5, 0xE1),  // bevelDark
    QColor(0xFF, 0xFF, 0xFF),  // canvasNeutral
    QColor(0xF8, 0xFA, 0xFC),  // desk
    QColor(0xDC, 0x26, 0x26),  // danger
    QColor(0x16, 0xA3, 0x4A),  // ok
};

class ChromeStyle : public QProxyStyle {
public:
  ChromeStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

  int pixelMetric(PixelMetric metric, const QStyleOption* opt, const QWidget* w) const override {
    if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
        metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
      return 14;
    return QProxyStyle::pixelMetric(metric, opt, w);
  }

  void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                     const QWidget* w) const override {
    if (!opt || !p) {
      QProxyStyle::drawPrimitive(pe, opt, p, w);
      return;
    }
    if (pe == PE_IndicatorCheckBox || pe == PE_IndicatorItemViewItemCheck) {
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      const QRectF r = QRectF(opt->rect).adjusted(1.0, 1.0, -1.0, -1.0);
      const bool on = opt->state.testFlag(State_On);
      const bool part = opt->state.testFlag(State_NoChange);
      const bool dis = !opt->state.testFlag(State_Enabled);
      p->setPen(QPen(dis ? QColor(0x94, 0xA3, 0xB8) : QColor(0x94, 0xA3, 0xB8), 1.0));
      p->setBrush(on || part ? QColor(0x25, 0x63, 0xEB) : QColor(255, 255, 255));
      p->drawRoundedRect(r, 2.0, 2.0);
      if (on) {
        QPainterPath tick;
        tick.moveTo(r.left() + r.width() * 0.20, r.center().y());
        tick.lineTo(r.left() + r.width() * 0.42, r.bottom() - r.height() * 0.26);
        tick.lineTo(r.right() - r.width() * 0.18, r.top() + r.height() * 0.24);
        p->setPen(QPen(QColor(255, 255, 255), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(tick);
      } else if (part) {
        p->fillRect(r.adjusted(3, 3, -3, -3), QColor(255, 255, 255));
      }
      p->restore();
      return;
    }
    if (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown || pe == PE_IndicatorArrowUp ||
        pe == PE_IndicatorArrowDown) {
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      const QRect r = opt->rect;
      const bool up = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorArrowUp);
      const qreal cx = r.center().x();
      const qreal cy = r.center().y();
      QPainterPath path;
      if (up) {
        path.moveTo(cx, cy - 3.6);
        path.lineTo(cx + 5.2, cy + 2.6);
        path.lineTo(cx - 5.2, cy + 2.6);
      } else {
        path.moveTo(cx, cy + 3.6);
        path.lineTo(cx + 5.2, cy - 2.6);
        path.lineTo(cx - 5.2, cy - 2.6);
      }
      path.closeSubpath();
      p->setPen(Qt::NoPen);
      p->setBrush(opt->state.testFlag(State_Enabled) ? QColor(0x1E, 0x29, 0x3B)
                                                     : QColor(0x94, 0xA3, 0xB8));
      p->drawPath(path);
      p->restore();
      return;
    }
    QProxyStyle::drawPrimitive(pe, opt, p, w);
  }
};

void setGroup(QPalette& pal, QPalette::ColorGroup g, const Tokens& t, bool disabled) {
  const QColor text = disabled ? t.inkDisabled : t.ink;
  pal.setColor(g, QPalette::Window, t.sky4);
  pal.setColor(g, QPalette::WindowText, text);
  pal.setColor(g, QPalette::Base, disabled ? t.sky0 : Qt::white);
  pal.setColor(g, QPalette::AlternateBase, disabled ? t.sky2 : t.sky0);
  pal.setColor(g, QPalette::Text, text);
  pal.setColor(g, QPalette::Button, disabled ? t.sky4 : Qt::white);
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
  const QStringList fontCands = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/fonts/PretendardGOV-Regular.otf")),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/fonts/PretendardGOV-Regular.otf")),
      QDir::current().filePath(QStringLiteral("data/fonts/PretendardGOV-Regular.otf")),
  };
  for (const QString& fp : fontCands) {
    if (QFile::exists(fp) && QFontDatabase::addApplicationFont(fp) >= 0)
      break;
  }
  QFont ui(QStringLiteral("Pretendard GOV"));
  if (ui.exactMatch() || QFontInfo(ui).family().contains(QLatin1String("Pretendard"))) {
    ui.setPixelSize(13);
    app->setFont(ui);
  } else {
    QFont fallback(QStringLiteral("Malgun Gothic"));
    fallback.setPixelSize(13);
    app->setFont(fallback);
  }
  app->setStyle(new ChromeStyle);
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
  return QStringLiteral("background-color: %1; border: 1px solid #E5E7EB; border-radius: 8px;")
      .arg(use.name());
}

}  // namespace KaTheme
