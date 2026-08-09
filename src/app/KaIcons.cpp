#include "KaIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QHash>
#include <QApplication>
#include <QStyle>

namespace {

QPixmap base(int s = 64) {
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  return pm;
}

QIcon fromPixmap(const QPixmap& pm) {
  return QIcon(pm);
}

void roundBg(QPainter& p, const QRectF& r, const QColor& c) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(c);
  p.drawRoundedRect(r, r.width() * 0.22, r.height() * 0.22);
}

QIcon drawDocPlus() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(37, 99, 235));
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(18, 12, 28, 36), 3, 3);
  p.setPen(QPen(QColor(37, 99, 235), 3.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(32, 24), QPointF(32, 40));
  p.drawLine(QPointF(24, 32), QPointF(40, 32));
  return fromPixmap(pm);
}

QIcon drawFolderOpen() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(217, 119, 6));
  p.setBrush(QColor(254, 243, 199));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(12, 24, 40, 26), 3, 3);
  p.setBrush(QColor(253, 230, 138));
  p.drawRoundedRect(QRectF(12, 18, 18, 10), 2, 2);
  return fromPixmap(pm);
}

QIcon drawSave() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(22, 163, 74));
  p.setBrush(QColor(220, 252, 231));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(16, 14, 32, 36), 3, 3);
  p.setBrush(QColor(22, 163, 74));
  p.drawRect(QRectF(22, 14, 20, 12));
  p.setBrush(Qt::white);
  p.drawRect(QRectF(24, 32, 16, 12));
  return fromPixmap(pm);
}

QIcon drawLayer() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(79, 70, 229));
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(199, 210, 254));
  QPolygonF a; a << QPointF(32, 14) << QPointF(50, 24) << QPointF(32, 34) << QPointF(14, 24);
  p.drawPolygon(a);
  p.setBrush(QColor(165, 180, 252));
  QPolygonF b; b << QPointF(32, 24) << QPointF(50, 34) << QPointF(32, 44) << QPointF(14, 34);
  p.drawPolygon(b);
  p.setBrush(QColor(129, 140, 248));
  QPolygonF c; c << QPointF(32, 34) << QPointF(50, 44) << QPointF(32, 54) << QPointF(14, 44);
  p.drawPolygon(c);
  return fromPixmap(pm);
}

QIcon drawMap() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(14, 165, 233));
  p.setBrush(QColor(224, 242, 254));
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
  p.setPen(QPen(QColor(2, 132, 199), 2));
  p.drawLine(26, 16, 26, 48);
  p.drawLine(38, 16, 38, 48);
  p.drawLine(16, 26, 48, 26);
  p.drawLine(16, 38, 48, 38);
  p.setBrush(QColor(239, 68, 68));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(34, 30), 5, 5);
  return fromPixmap(pm);
}

QIcon drawSatellite() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(30, 41, 59));
  p.setBrush(QColor(148, 163, 184));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(32, 34), 14, 14);
  p.setBrush(QColor(56, 189, 248));
  p.drawEllipse(QPointF(28, 30), 5, 4);
  p.setBrush(QColor(34, 197, 94));
  p.drawEllipse(QPointF(38, 36), 4, 5);
  p.setPen(QPen(Qt::white, 2));
  p.drawLine(20, 16, 28, 22);
  p.drawLine(44, 16, 36, 22);
  return fromPixmap(pm);
}

QIcon drawPolygon() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(234, 88, 12));
  p.setBrush(QColor(254, 215, 170));
  p.setPen(QPen(Qt::white, 2.5));
  QPolygonF poly;
  poly << QPointF(18, 40) << QPointF(22, 18) << QPointF(42, 16) << QPointF(48, 34) << QPointF(34, 48);
  p.drawPolygon(poly);
  return fromPixmap(pm);
}

QIcon drawLine() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(190, 24, 93));
  p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(16, 44, 28, 20);
  p.drawLine(28, 20, 48, 36);
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(16, 44), 4, 4);
  p.drawEllipse(QPointF(28, 20), 4, 4);
  p.drawEllipse(QPointF(48, 36), 4, 4);
  return fromPixmap(pm);
}

QIcon drawGps() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(13, 148, 136));
  p.setBrush(QColor(204, 251, 241));
  p.setPen(Qt::NoPen);
  QPainterPath pin;
  pin.moveTo(32, 48);
  pin.cubicTo(18, 34, 18, 22, 32, 18);
  pin.cubicTo(46, 22, 46, 34, 32, 48);
  p.drawPath(pin);
  p.setBrush(QColor(13, 148, 136));
  p.drawEllipse(QPointF(32, 28), 5, 5);
  return fromPixmap(pm);
}

QIcon drawCheck() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(22, 163, 74));
  p.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(18, 34, 28, 44);
  p.drawLine(28, 44, 46, 20);
  return fromPixmap(pm);
}

QIcon drawExport() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(124, 58, 237));
  p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(32, 16, 32, 38);
  p.drawLine(22, 30, 32, 40);
  p.drawLine(42, 30, 32, 40);
  p.drawLine(18, 46, 46, 46);
  return fromPixmap(pm);
}

QIcon drawPdf() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(220, 38, 38));
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(18, 12, 28, 40), 3, 3);
  p.setPen(QPen(QColor(220, 38, 38), 2));
  p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::Bold));
  p.drawText(QRectF(18, 24, 28, 20), Qt::AlignCenter, QStringLiteral("PDF"));
  return fromPixmap(pm);
}

QIcon drawCrs() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(8, 145, 178));
  p.setPen(QPen(Qt::white, 2.5));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(32, 32), 16, 16);
  p.drawEllipse(QPointF(32, 32), 8, 16);
  p.drawLine(16, 32, 48, 32);
  p.drawLine(32, 16, 32, 48);
  return fromPixmap(pm);
}

QIcon drawTransform() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(67, 56, 202));
  p.setPen(QPen(Qt::white, 3.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(16, 24, 40, 24);
  p.drawLine(32, 16, 42, 24);
  p.drawLine(32, 32, 42, 24);
  p.drawLine(48, 40, 24, 40);
  p.drawLine(32, 32, 22, 40);
  p.drawLine(32, 48, 22, 40);
  return fromPixmap(pm);
}

QIcon drawUpload() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(5, 150, 105));
  p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(32, 42, 32, 18);
  p.drawLine(22, 28, 32, 16);
  p.drawLine(42, 28, 32, 16);
  p.drawLine(18, 46, 46, 46);
  return fromPixmap(pm);
}

QIcon drawTrash() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(185, 28, 28));
  p.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(20, 22, 44, 22);
  p.drawLine(26, 18, 38, 18);
  p.drawRect(QRectF(22, 22, 20, 26));
  p.drawLine(28, 28, 28, 42);
  p.drawLine(36, 28, 36, 42);
  return fromPixmap(pm);
}

QIcon drawGeoref() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(161, 98, 7));
  p.setBrush(QColor(254, 243, 199));
  p.setPen(QPen(Qt::white, 2));
  p.drawRect(QRectF(14, 16, 28, 28));
  p.setBrush(QColor(239, 68, 68));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(20, 22), 3, 3);
  p.drawEllipse(QPointF(36, 22), 3, 3);
  p.drawEllipse(QPointF(20, 38), 3, 3);
  p.drawEllipse(QPointF(40, 40), 3, 3);
  return fromPixmap(pm);
}

QIcon drawPalette() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(147, 51, 234));
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(248, 113, 113)); p.drawEllipse(QPointF(24, 26), 7, 7);
  p.setBrush(QColor(74, 222, 128)); p.drawEllipse(QPointF(40, 26), 7, 7);
  p.setBrush(QColor(96, 165, 250)); p.drawEllipse(QPointF(24, 42), 7, 7);
  p.setBrush(QColor(251, 191, 36)); p.drawEllipse(QPointF(40, 42), 7, 7);
  return fromPixmap(pm);
}

QIcon drawSaveEdit() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(5, 150, 105));
  p.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(18, 34, 28, 44);
  p.drawLine(28, 44, 46, 20);
  return fromPixmap(pm);
}

QIcon drawStop() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(75, 85, 99));
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(QRectF(22, 22, 20, 20), 3, 3);
  return fromPixmap(pm);
}

QIcon drawHelp() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(59, 130, 246));
  p.setPen(Qt::NoPen);
  p.setBrush(Qt::white);
  p.setFont(QFont(QStringLiteral("Segoe UI"), 28, QFont::Bold));
  p.drawText(QRectF(4, 4, 56, 56), Qt::AlignCenter, QStringLiteral("?"));
  return fromPixmap(pm);
}

QIcon drawSearch() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(14, 116, 144));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(Qt::white, 4));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(28, 28), 12, 12);
  p.drawLine(QPointF(37, 37), QPointF(48, 48));
  return fromPixmap(pm);
}

QIcon drawToolPolygon() {
  auto pm = base();
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(249, 115, 22));
  p.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(QColor(255, 255, 255, 80));
  QPolygonF poly;
  poly << QPointF(16, 42) << QPointF(20, 18) << QPointF(40, 14) << QPointF(48, 36) << QPointF(30, 48);
  p.drawPolygon(poly);
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  for (const QPointF& pt : poly) p.drawEllipse(pt, 3.2, 3.2);
  p.setPen(QPen(QColor(15, 23, 42), 2.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(38, 40), QPointF(50, 50));
  p.setBrush(QColor(15, 23, 42));
  QPolygonF tip;
  tip << QPointF(50, 50) << QPointF(44, 50) << QPointF(50, 44);
  p.drawPolygon(tip);
  return fromPixmap(pm);
}

QIcon drawToolLine() {
  auto pm = base();
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(219, 39, 119));
  p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawLine(14, 44, 26, 18);
  p.drawLine(26, 18, 46, 34);
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(14, 44), 4, 4);
  p.drawEllipse(QPointF(26, 18), 4, 4);
  p.drawEllipse(QPointF(46, 34), 4, 4);
  p.setPen(QPen(QColor(15, 23, 42), 2.5, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(QPointF(36, 42), QPointF(50, 52));
  p.setBrush(QColor(15, 23, 42));
  QPolygonF tip;
  tip << QPointF(50, 52) << QPointF(44, 52) << QPointF(50, 46);
  p.drawPolygon(tip);
  return fromPixmap(pm);
}

QIcon drawToolArea() {
  auto pm = base();
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(8, 145, 178));
  p.setPen(QPen(Qt::white, 3));
  p.setBrush(QColor(165, 243, 252, 120));
  p.drawRoundedRect(QRectF(14, 16, 28, 28), 3, 3);
  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(14, 16), 3, 3);
  p.drawEllipse(QPointF(42, 16), 3, 3);
  p.drawEllipse(QPointF(42, 44), 3, 3);
  p.drawEllipse(QPointF(14, 44), 3, 3);
  p.setPen(QPen(QColor(15, 23, 42), 2.5));
  p.drawLine(QPointF(34, 40), QPointF(50, 50));
  return fromPixmap(pm);
}

QIcon drawStep(int n, const QColor& bg) {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), bg);
  p.setPen(Qt::white);
  p.setFont(QFont(QStringLiteral("Segoe UI"), 22, QFont::Bold));
  p.drawText(QRectF(4, 4, 56, 56), Qt::AlignCenter, QString::number(n));
  return fromPixmap(pm);
}

QIcon styleIcon(QStyle::StandardPixmap sp) {
  if (QApplication::style())
    return QApplication::style()->standardIcon(sp);
  return {};
}

QIcon drawCadastral() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(180, 83, 9));
  p.setBrush(QColor(254, 243, 199));
  p.setPen(QPen(QColor(217, 119, 6), 2));
  p.drawRect(QRectF(14, 14, 36, 36));
  p.setPen(QPen(QColor(180, 83, 9), 2, Qt::DashLine));
  p.drawLine(14, 28, 50, 28);
  p.drawLine(32, 14, 32, 50);
  p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::Bold));
  p.drawText(QRectF(14, 14, 36, 36), Qt::AlignCenter, QStringLiteral("지적"));
  return fromPixmap(pm);
}

QIcon drawContour() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(15, 118, 110));
  p.setPen(QPen(QColor(204, 251, 241), 2));
  p.drawArc(QRectF(12, 12, 40, 40), 0, 180 * 16);
  p.drawArc(QRectF(18, 22, 28, 28), 0, 180 * 16);
  p.drawArc(QRectF(24, 32, 16, 16), 0, 180 * 16);
  p.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::Bold));
  p.drawText(QRectF(4, 36, 56, 20), Qt::AlignCenter, QStringLiteral("등고선"));
  return fromPixmap(pm);
}

QIcon drawDarkMode() {
  auto pm = base();
  QPainter p(&pm);
  roundBg(p, QRectF(4, 4, 56, 56), QColor(49, 46, 129));
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(253, 224, 71));
  p.drawEllipse(QRectF(16, 16, 32, 32));
  p.setBrush(QColor(49, 46, 129));
  p.drawEllipse(QRectF(24, 12, 28, 28));
  return fromPixmap(pm);
}

}

namespace KaIcons {

QIcon icon(const QString& id) {
  static QHash<QString, QIcon> cache;
  if (cache.contains(id)) return cache.value(id);

  QIcon ic;
  if (id == QLatin1String("new")) ic = drawDocPlus();
  else if (id == QLatin1String("open")) ic = drawFolderOpen();
  else if (id == QLatin1String("save")) ic = drawSave();
  else if (id == QLatin1String("layer")) ic = drawLayer();
  else if (id == QLatin1String("map") || id == QLatin1String("vworld_base")) ic = drawMap();
  else if (id == QLatin1String("satellite") || id == QLatin1String("vworld_sat")) ic = drawSatellite();
  else if (id == QLatin1String("vworld_cadastral") || id == QLatin1String("cadastral")) ic = drawCadastral();
  else if (id == QLatin1String("vworld_hybrid") || id == QLatin1String("hybrid")) ic = drawMap();
  else if (id == QLatin1String("vworld_contour") || id == QLatin1String("contour")) ic = drawContour();
  else if (id == QLatin1String("dark_mode")) ic = drawDarkMode();
  else if (id == QLatin1String("polygon") || id == QLatin1String("survey_area")) ic = drawPolygon();
  else if (id == QLatin1String("line") || id == QLatin1String("feature_line")) ic = drawLine();
  else if (id == QLatin1String("feature_poly")) ic = drawToolPolygon();
  else if (id == QLatin1String("gps")) ic = drawGps();
  else if (id == QLatin1String("check")) ic = drawCheck();
  else if (id == QLatin1String("export")) ic = drawExport();
  else if (id == QLatin1String("pdf")) ic = drawPdf();
  else if (id == QLatin1String("crs")) ic = drawCrs();
  else if (id == QLatin1String("transform")) ic = drawTransform();
  else if (id == QLatin1String("upload")) ic = drawUpload();
  else if (id == QLatin1String("trash")) ic = drawTrash();
  else if (id == QLatin1String("georef")) ic = drawGeoref();
  else if (id == QLatin1String("palette")) ic = drawPalette();
  else if (id == QLatin1String("saveedit")) ic = drawSaveEdit();
  else if (id == QLatin1String("stop")) ic = drawStop();
  else if (id == QLatin1String("help")) ic = drawHelp();
  else if (id == QLatin1String("search")) ic = drawSearch();
  else if (id == QLatin1String("draw_poly")) ic = drawToolPolygon();
  else if (id == QLatin1String("draw_line")) ic = drawToolLine();
  else if (id == QLatin1String("draw_area")) ic = drawToolArea();
  else if (id == QLatin1String("import")) ic = drawFolderOpen();
  else ic = styleIcon(QStyle::SP_FileIcon);

  cache.insert(id, ic);
  return ic;
}

}
