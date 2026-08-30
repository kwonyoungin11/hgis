#include "KaTheme.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>
#include <QWidget>

namespace KaTheme {
namespace {

// Anthropic-inspired palette: warm ivory paper, warm ink, terracotta accent.
const Tokens kTokens = {
    QColor(0xE8, 0xF0, 0xFA),  // sky0 pale blue wash
    QColor(0x1E, 0x67, 0xC6),  // sky1 primary deep blue
    QColor(0x17, 0x5A, 0xB0),  // sky2 hover
    QColor(0x12, 0x4B, 0x94),  // sky3 deep
    QColor(0xF2, 0xF3, 0xF5),  // sky4 cool light gray
    QColor(0x1E, 0x67, 0xC6),  // sky5 highlight
    QColor(0x1F, 0x23, 0x28),  // sky6 ink
    QColor(0x1F, 0x23, 0x28),  // ink
    QColor(0x6E, 0x75, 0x7D),  // inkMuted
    QColor(0xA5, 0xAB, 0xB3),  // inkDisabled
    QColor(0xD5, 0xD9, 0xDE),  // border
    QColor(0xFF, 0xFF, 0xFF),  // bevelLight
    QColor(0xC3, 0xC8, 0xCF),  // bevelDark
    QColor(0xFF, 0xFF, 0xFF),  // canvasNeutral
    QColor(0xF2, 0xF3, 0xF5),  // desk
    QColor(0xC0, 0x3A, 0x2B),  // danger
    QColor(0x2E, 0x7D, 0x4F),  // ok
};

class ChromeStyle : public QProxyStyle {
public:
  ChromeStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

  int pixelMetric(PixelMetric metric, const QStyleOption* opt, const QWidget* w) const override {
    if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
        metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
      return 16;
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
      QRect box = opt->rect;
      if (box.width() < 14 || box.height() < 14)
        box = QRect(box.center().x() - 8, box.center().y() - 8, 16, 16);
      const QRectF r = QRectF(box).adjusted(1.0, 1.0, -1.0, -1.0);
      const bool on = opt->state.testFlag(State_On);
      const bool part = opt->state.testFlag(State_NoChange);
      const bool dis = !opt->state.testFlag(State_Enabled);
      const bool hover = opt->state.testFlag(State_MouseOver);
      const QColor fill(0x1E, 0x67, 0xC6);
      const QColor edge(hover ? QColor(0x12, 0x4B, 0x94) : QColor(0x17, 0x5A, 0xB0));
      const QColor tickInk(0xFF, 0xFF, 0xFF);
      const QColor stone(hover ? QColor(0x17, 0x5A, 0xB0) : QColor(0xB9, 0xBF, 0xC7));
      p->setPen(QPen(dis ? QColor(0xC3, 0xC8, 0xCF)
                         : ((on || part) ? edge : stone),
                     1.1));
      p->setBrush(dis ? QColor(0xF2, 0xF3, 0xF5)
                      : ((on || part) ? fill : QColor(255, 255, 255)));
      p->drawRoundedRect(r, 3.5, 3.5);
      if (on) {
        QPainterPath tick;
        tick.moveTo(r.left() + r.width() * 0.22, r.center().y() + r.height() * 0.02);
        tick.lineTo(r.left() + r.width() * 0.40, r.bottom() - r.height() * 0.28);
        tick.lineTo(r.right() - r.width() * 0.20, r.top() + r.height() * 0.26);
        p->setPen(QPen(tickInk, 1.85, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(tick);
      } else if (part) {
        const QRectF bar = r.adjusted(r.width() * 0.22, r.height() * 0.42,
                                      -r.width() * 0.22, -r.height() * 0.42);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255));
        p->drawRoundedRect(bar, 1.2, 1.2);
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
      p->setBrush(opt->state.testFlag(State_Enabled) ? QColor(0x2A, 0x31, 0x38)
                                                     : QColor(0xA5, 0xAB, 0xB3));
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
  pal.setColor(g, QPalette::PlaceholderText, disabled ? QColor(0xA5, 0xAB, 0xB3) : t.inkDisabled);
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
  // Field PCs ship Malgun Gothic. Pretendard is not in data/fonts — do not
  // put it first in QSS or QFont or Hangul falls back to a Latin substitute.
  QFont ui(QStringLiteral("Malgun Gothic"));
  ui.setPixelSize(13);
  ui.setHintingPreference(QFont::PreferFullHinting);
  ui.setStyleStrategy(QFont::PreferAntialias);
  app->setFont(ui);
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
