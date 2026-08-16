#include "KaIcons.h"
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

const QColor kInk(0x1E, 0x29, 0x3B);
thread_local QColor tInk = kInk;

QPixmap base(int s = 64) {
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  return pm;
}

void prep(QPainter& p, qreal width = 3.0) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(tInk, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
}

void fillInk(QPainter& p) { p.setBrush(tInk); }

QIcon bake(void (*fn)(QPainter&)) {
  auto pmAt = [&](const QColor& c) {
    tInk = c;
    auto pm = base();
    QPainter p(&pm);
    fn(p);
    return pm;
  };
  QIcon ic;
  ic.addPixmap(pmAt(kInk), QIcon::Normal, QIcon::Off);
  ic.addPixmap(pmAt(Qt::white), QIcon::Normal, QIcon::On);
  ic.addPixmap(pmAt(Qt::white), QIcon::Selected);
  ic.addPixmap(pmAt(kInk), QIcon::Active);
  return ic;
}

void dDocPlus(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(18, 12, 28, 38), 4, 4);
  p.drawLine(QPointF(32, 24), QPointF(32, 40));
  p.drawLine(QPointF(24, 32), QPointF(40, 32));
}

void dFolder(QPainter& p) {
  prep(p, 3.0);
  QPainterPath path;
  path.moveTo(12, 24);
  path.lineTo(12, 20);
  path.quadTo(12, 16, 16, 16);
  path.lineTo(28, 16);
  path.lineTo(32, 22);
  path.lineTo(48, 22);
  path.quadTo(52, 22, 52, 26);
  path.lineTo(52, 46);
  path.quadTo(52, 50, 48, 50);
  path.lineTo(16, 50);
  path.quadTo(12, 50, 12, 46);
  path.closeSubpath();
  p.drawPath(path);
}

void dSave(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(16, 14, 32, 36), 3, 3);
  p.drawRect(QRectF(22, 14, 20, 12));
  p.drawRect(QRectF(24, 34, 16, 12));
}

void dLayer(QPainter& p) {
  prep(p, 2.6);
  auto plate = [](qreal y) {
    QPolygonF a;
    a << QPointF(32, y) << QPointF(50, y + 8) << QPointF(32, y + 16) << QPointF(14, y + 8);
    return a;
  };
  p.drawPolygon(plate(14));
  p.drawPolygon(plate(24));
  p.drawPolygon(plate(34));
}

void dMap(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  p.drawLine(26, 16, 26, 48);
  p.drawLine(38, 16, 38, 48);
  p.drawLine(16, 26, 48, 26);
  p.drawLine(16, 38, 48, 38);
  fillInk(p);
  p.drawEllipse(QPointF(34, 30), 3.5, 3.5);
}

void dSatellite(QPainter& p) {
  prep(p, 2.8);
  p.drawEllipse(QPointF(32, 34), 14, 14);
  p.drawEllipse(QPointF(32, 34), 6, 14);
  p.drawLine(18, 34, 46, 34);
  p.drawLine(20, 16, 28, 24);
  p.drawLine(44, 16, 36, 24);
}

void dPolygon(QPainter& p) {
  prep(p, 3.0);
  QPolygonF poly;
  poly << QPointF(18, 42) << QPointF(22, 16) << QPointF(44, 16) << QPointF(50, 34) << QPointF(34, 50);
  p.drawPolygon(poly);
}

void dLine(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(16, 46, 28, 18);
  p.drawLine(28, 18, 50, 36);
  fillInk(p);
  p.drawEllipse(QPointF(16, 46), 3.2, 3.2);
  p.drawEllipse(QPointF(28, 18), 3.2, 3.2);
  p.drawEllipse(QPointF(50, 36), 3.2, 3.2);
}

void dGps(QPainter& p) {
  prep(p, 3.0);
  QPainterPath pin;
  pin.moveTo(32, 50);
  pin.cubicTo(16, 34, 16, 20, 32, 16);
  pin.cubicTo(48, 20, 48, 34, 32, 50);
  p.drawPath(pin);
  p.drawEllipse(QPointF(32, 28), 5, 5);
}

void dCheck(QPainter& p) {
  prep(p, 4.0);
  p.drawLine(16, 34, 28, 46);
  p.drawLine(28, 46, 50, 18);
}

void dExport(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(32, 14, 32, 38);
  p.drawLine(22, 28, 32, 40);
  p.drawLine(42, 28, 32, 40);
  p.drawLine(16, 48, 48, 48);
}

void dPdf(QPainter& p) {
  prep(p, 2.8);
  p.drawRoundedRect(QRectF(16, 10, 32, 44), 4, 4);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 10, QFont::Bold));
  p.drawText(QRectF(16, 24, 32, 20), Qt::AlignCenter, QStringLiteral("PDF"));
}

void dCrs(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawEllipse(QPointF(32, 32), 8, 16);
  p.drawLine(16, 32, 48, 32);
  p.drawLine(32, 16, 32, 48);
}

void dTransform(QPainter& p) {
  prep(p, 3.0);
  p.drawLine(14, 22, 42, 22);
  p.drawLine(34, 14, 44, 22);
  p.drawLine(34, 30, 44, 22);
  p.drawLine(50, 42, 22, 42);
  p.drawLine(32, 34, 20, 42);
  p.drawLine(32, 50, 20, 42);
}

void dUpload(QPainter& p) {
  prep(p, 3.2);
  p.drawLine(32, 44, 32, 16);
  p.drawLine(20, 28, 32, 14);
  p.drawLine(44, 28, 32, 14);
  p.drawLine(16, 48, 48, 48);
}

void dTrash(QPainter& p) {
  prep(p, 2.8);
  p.drawLine(18, 20, 46, 20);
  p.drawLine(26, 16, 38, 16);
  p.drawRoundedRect(QRectF(20, 20, 24, 28), 2, 2);
  p.drawLine(28, 26, 28, 40);
  p.drawLine(36, 26, 36, 40);
}

void dGeoref(QPainter& p) {
  prep(p, 2.6);
  p.drawRect(QRectF(14, 16, 28, 28));
  fillInk(p);
  p.drawEllipse(QPointF(20, 22), 2.6, 2.6);
  p.drawEllipse(QPointF(36, 22), 2.6, 2.6);
  p.drawEllipse(QPointF(20, 38), 2.6, 2.6);
  p.drawEllipse(QPointF(40, 40), 2.6, 2.6);
}

void dPalette(QPainter& p) {
  prep(p, 2.4);
  p.drawEllipse(QPointF(24, 26), 8, 8);
  p.drawEllipse(QPointF(40, 26), 8, 8);
  p.drawEllipse(QPointF(24, 42), 8, 8);
  p.drawEllipse(QPointF(40, 42), 8, 8);
}

void dStop(QPainter& p) {
  prep(p, 3.0);
  p.drawRoundedRect(QRectF(20, 20, 24, 24), 3, 3);
}

void dHelp(QPainter& p) {
  prep(p, 3.0);
  p.drawEllipse(QPointF(32, 32), 18, 18);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 22, QFont::Bold));
  p.drawText(QRectF(4, 4, 56, 56), Qt::AlignCenter, QStringLiteral("?"));
}

void dSearch(QPainter& p) {
  prep(p, 3.2);
  p.drawEllipse(QPointF(28, 28), 13, 13);
  p.drawLine(QPointF(38, 38), QPointF(50, 50));
}

void dSelect(QPainter& p) {
  prep(p, 2.6);
  QPainterPath arrow;
  arrow.moveTo(18, 12);
  arrow.lineTo(18, 48);
  arrow.lineTo(28, 36);
  arrow.lineTo(36, 52);
  arrow.lineTo(44, 48);
  arrow.lineTo(34, 32);
  arrow.lineTo(48, 32);
  arrow.closeSubpath();
  p.drawPath(arrow);
}

void dCadastral(QPainter& p) {
  prep(p, 2.6);
  p.drawRect(QRectF(14, 14, 36, 36));
  p.drawLine(14, 26, 50, 26);
  p.drawLine(14, 38, 50, 38);
  p.drawLine(26, 14, 26, 50);
  p.drawLine(38, 14, 38, 50);
}

void dContour(QPainter& p) {
  prep(p, 2.4);
  p.drawArc(QRectF(12, 12, 40, 40), 20 * 16, 140 * 16);
  p.drawArc(QRectF(18, 20, 28, 28), 20 * 16, 140 * 16);
  p.drawArc(QRectF(24, 28, 16, 16), 20 * 16, 140 * 16);
}

void dDark(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QRectF(16, 16, 32, 32));
  p.drawArc(QRectF(24, 14, 26, 28), 40 * 16, 200 * 16);
}

void dToolPoly(QPainter& p) {
  prep(p, 2.8);
  QPolygonF poly;
  poly << QPointF(14, 42) << QPointF(20, 16) << QPointF(40, 14) << QPointF(48, 34) << QPointF(30, 48);
  p.drawPolygon(poly);
  p.drawLine(QPointF(38, 40), QPointF(52, 52));
}

void dToolLine(QPainter& p) {
  prep(p, 3.0);
  p.drawLine(14, 44, 26, 16);
  p.drawLine(26, 16, 46, 34);
  fillInk(p);
  p.drawEllipse(QPointF(14, 44), 3, 3);
  p.drawEllipse(QPointF(26, 16), 3, 3);
  p.drawEllipse(QPointF(46, 34), 3, 3);
}

void dToolArea(QPainter& p) {
  prep(p, 2.8);
  p.drawRoundedRect(QRectF(14, 16, 28, 28), 3, 3);
}

void dArtifact(QPainter& p) {
  prep(p, 2.6);
  QPolygonF tri;
  tri << QPointF(32, 14) << QPointF(50, 48) << QPointF(14, 48);
  p.drawPolygon(tri);
}

void dSnap(QPainter& p) {
  prep(p, 2.8);
  QPainterPath mag;
  mag.moveTo(18, 16);
  mag.lineTo(18, 36);
  mag.quadTo(18, 50, 32, 50);
  mag.quadTo(46, 50, 46, 36);
  mag.lineTo(46, 16);
  p.drawPath(mag);
  p.drawLine(QPointF(14, 16), QPointF(22, 16));
  p.drawLine(QPointF(42, 16), QPointF(50, 16));
  fillInk(p);
  p.drawEllipse(QPointF(18, 14), 2.4, 2.4);
  p.drawEllipse(QPointF(46, 14), 2.4, 2.4);
}

void dLayoutFrame(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(16, 12, 32, 40), 3, 3);
  p.setPen(QPen(tInk, 2.2, Qt::DashLine, Qt::RoundCap));
  p.drawRect(QRectF(20, 18, 24, 20));
}

void dLayoutSelect(QPainter& p) {
  prep(p, 2.2);
  p.setPen(QPen(tInk, 2.2, Qt::DashLine));
  p.drawRect(QRectF(16, 16, 28, 24));
  p.setPen(QPen(tInk, 2.4));
  p.drawRect(QRectF(13, 13, 7, 7));
  p.drawRect(QRectF(40, 13, 7, 7));
  p.drawRect(QRectF(13, 36, 7, 7));
  p.drawRect(QRectF(40, 36, 7, 7));
}

void dLayoutPan(QPainter& p) {
  prep(p, 2.8);
  p.drawLine(32, 14, 32, 50);
  p.drawLine(14, 32, 50, 32);
  p.drawLine(32, 14, 26, 22);
  p.drawLine(32, 14, 38, 22);
  p.drawLine(32, 50, 26, 42);
  p.drawLine(32, 50, 38, 42);
  p.drawLine(14, 32, 22, 26);
  p.drawLine(14, 32, 22, 38);
  p.drawLine(50, 32, 42, 26);
  p.drawLine(50, 32, 42, 38);
}

void dLayoutZoom(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(18, 16, 28, 32), 3, 3);
  p.drawLine(12, 12, 20, 12);
  p.drawLine(12, 12, 12, 20);
  p.drawLine(52, 12, 44, 12);
  p.drawLine(52, 12, 52, 20);
  p.drawLine(12, 52, 12, 44);
  p.drawLine(12, 52, 20, 52);
  p.drawLine(52, 52, 52, 44);
  p.drawLine(52, 52, 44, 52);
}

void dNorth(QPainter& p) {
  prep(p, 2.6);
  QPolygonF up;
  up << QPointF(32, 10) << QPointF(44, 38) << QPointF(32, 30) << QPointF(20, 38);
  p.drawPolygon(up);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
  p.drawText(QRectF(18, 40, 28, 16), Qt::AlignCenter, QStringLiteral("N"));
}

void dScaleBar(QPainter& p) {
  prep(p, 2.4);
  p.drawRect(QRectF(12, 26, 40, 12));
  p.drawLine(22, 26, 22, 38);
  p.drawLine(32, 26, 32, 38);
  p.drawLine(42, 26, 42, 38);
}

void dScaleText(QPainter& p) {
  prep(p, 2.4);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
  p.drawText(QRectF(6, 16, 52, 32), Qt::AlignCenter, QStringLiteral("1:n"));
}

void dLegend(QPainter& p) {
  prep(p, 2.4);
  p.drawRoundedRect(QRectF(14, 16, 10, 10), 2, 2);
  p.drawRoundedRect(QRectF(14, 30, 10, 10), 2, 2);
  p.drawRoundedRect(QRectF(14, 44, 10, 8), 2, 2);
  p.drawLine(30, 21, 50, 21);
  p.drawLine(30, 35, 50, 35);
  p.drawLine(30, 48, 46, 48);
}

void dActivate(QPainter& p) {
  prep(p, 2.6);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  p.drawLine(22, 40, 40, 22);
  fillInk(p);
  p.drawEllipse(QPointF(40, 22), 3, 3);
}

void dCenter(QPainter& p) {
  prep(p, 2.6);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawLine(32, 12, 32, 22);
  p.drawLine(32, 42, 32, 52);
  p.drawLine(12, 32, 22, 32);
  p.drawLine(42, 32, 52, 32);
  fillInk(p);
  p.drawEllipse(QPointF(32, 32), 3, 3);
}

}  // namespace

namespace KaIcons {

QIcon icon(const QString& id) {
  static QHash<QString, QIcon> cache;
  if (cache.contains(id)) return cache.value(id);

  QIcon ic;
  if (id == QLatin1String("new")) ic = bake(dDocPlus);
  else if (id == QLatin1String("open") || id == QLatin1String("import")) ic = bake(dFolder);
  else if (id == QLatin1String("save")) ic = bake(dSave);
  else if (id == QLatin1String("layer")) ic = bake(dLayer);
  else if (id == QLatin1String("map") || id == QLatin1String("vworld_base") ||
           id == QLatin1String("vworld_hybrid") || id == QLatin1String("hybrid"))
    ic = bake(dMap);
  else if (id == QLatin1String("satellite") || id == QLatin1String("vworld_sat")) ic = bake(dSatellite);
  else if (id == QLatin1String("vworld_cadastral") || id == QLatin1String("cadastral")) ic = bake(dCadastral);
  else if (id == QLatin1String("vworld_contour") || id == QLatin1String("contour")) ic = bake(dContour);
  else if (id == QLatin1String("dark_mode")) ic = bake(dDark);
  else if (id == QLatin1String("polygon") || id == QLatin1String("survey_area")) ic = bake(dPolygon);
  else if (id == QLatin1String("line") || id == QLatin1String("feature_line")) ic = bake(dLine);
  else if (id == QLatin1String("feature_poly") || id == QLatin1String("draw_poly")) ic = bake(dToolPoly);
  else if (id == QLatin1String("gps")) ic = bake(dGps);
  else if (id == QLatin1String("check") || id == QLatin1String("saveedit") ||
           id == QLatin1String("layout_activate_done"))
    ic = bake(dCheck);
  else if (id == QLatin1String("export")) ic = bake(dExport);
  else if (id == QLatin1String("pdf")) ic = bake(dPdf);
  else if (id == QLatin1String("crs")) ic = bake(dCrs);
  else if (id == QLatin1String("transform")) ic = bake(dTransform);
  else if (id == QLatin1String("upload")) ic = bake(dUpload);
  else if (id == QLatin1String("trash")) ic = bake(dTrash);
  else if (id == QLatin1String("georef")) ic = bake(dGeoref);
  else if (id == QLatin1String("palette")) ic = bake(dPalette);
  else if (id == QLatin1String("stop")) ic = bake(dStop);
  else if (id == QLatin1String("help")) ic = bake(dHelp);
  else if (id == QLatin1String("search")) ic = bake(dSearch);
  else if (id == QLatin1String("draw_line")) ic = bake(dToolLine);
  else if (id == QLatin1String("draw_area")) ic = bake(dToolArea);
  else if (id == QLatin1String("snap")) ic = bake(dSnap);
  else if (id == QLatin1String("artifact")) ic = bake(dArtifact);
  else if (id == QLatin1String("select") || id == QLatin1String("arrow")) ic = bake(dSelect);
  else if (id == QLatin1String("layout_map_frame")) ic = bake(dLayoutFrame);
  else if (id == QLatin1String("layout_select")) ic = bake(dLayoutSelect);
  else if (id == QLatin1String("layout_pan")) ic = bake(dLayoutPan);
  else if (id == QLatin1String("layout_zoom_full")) ic = bake(dLayoutZoom);
  else if (id == QLatin1String("layout_north")) ic = bake(dNorth);
  else if (id == QLatin1String("layout_scalebar")) ic = bake(dScaleBar);
  else if (id == QLatin1String("layout_scale")) ic = bake(dScaleText);
  else if (id == QLatin1String("layout_legend")) ic = bake(dLegend);
  else if (id == QLatin1String("layout_activate")) ic = bake(dActivate);
  else if (id == QLatin1String("layout_center")) ic = bake(dCenter);
  else ic = bake(dDocPlus);

  cache.insert(id, ic);
  return ic;
}

QIcon icon(const QString& id, const QColor& ink) {
  const QIcon base = icon(id);
  if (!ink.isValid() || ink == kInk) return base;
  const QPixmap src = base.pixmap(64, 64);
  QPixmap out(src.size());
  out.fill(Qt::transparent);
  QPainter p(&out);
  p.drawPixmap(0, 0, src);
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.fillRect(out.rect(), ink);
  QIcon tinted;
  tinted.addPixmap(out);
  return tinted;
}

}  // namespace KaIcons
