#include "FeaturePresets.h"

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QColor>

#include <qgis.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgssymbol.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
#include <qgssymbollayer.h>
#include <qgsproperty.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsrenderer.h>

FeaturePresets& FeaturePresets::instance() {
  static FeaturePresets s;
  return s;
}

bool FeaturePresets::load(const QString& jsonPath) {
  QFile f(jsonPath);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) return false;
  const QJsonObject root = doc.object();
  m_kinds.clear();
  m_periods.clear();
  for (const QJsonValue& v : root.value(QStringLiteral("kinds")).toArray()) {
    const QJsonObject o = v.toObject();
    Kind k;
    k.id = o.value(QStringLiteral("id")).toString();
    k.label = o.value(QStringLiteral("label")).toString();
    k.pattern = o.value(QStringLiteral("pattern")).toString(QStringLiteral("solid"));
    k.marker = o.value(QStringLiteral("marker")).toString(QStringLiteral("circle"));
    if (!k.label.isEmpty()) m_kinds.append(k);
  }
  for (const QJsonValue& v : root.value(QStringLiteral("periods")).toArray()) {
    const QJsonObject o = v.toObject();
    Period p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.label = o.value(QStringLiteral("label")).toString();
    p.color = o.value(QStringLiteral("color")).toString(QStringLiteral("#64748B"));
    if (!p.label.isEmpty()) m_periods.append(p);
  }
  return isLoaded();
}

bool FeaturePresets::ensureLoaded() {
  if (isLoaded()) return true;
  if (m_tried && !isLoaded()) return false;
  m_tried = true;
  const QStringList candidates = {
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("data/styles/feature_presets.json")),
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("../data/styles/feature_presets.json")),
      QDir::current().filePath(QStringLiteral("data/styles/feature_presets.json")),
      QStringLiteral("D:/qgis/data/styles/feature_presets.json"),
  };
  for (const QString& p : candidates) {
    if (QFile::exists(p) && load(p)) return true;
  }
  return false;
}

QString FeaturePresets::defaultKindLabel() const {
  for (const Kind& k : m_kinds) {
    if (k.id == QLatin1String("other")) return k.label;
  }
  return m_kinds.isEmpty() ? QStringLiteral("기타") : m_kinds.last().label;
}

QString FeaturePresets::defaultPeriodLabel() const {
  for (const Period& p : m_periods) {
    if (p.id == QLatin1String("unknown")) return p.label;
  }
  return m_periods.isEmpty() ? QStringLiteral("미정") : m_periods.last().label;
}

QString FeaturePresets::periodColorExpression(int alpha) const {
  QString expr = QStringLiteral("CASE");
  for (const Period& p : m_periods) {
    const QColor c(p.color);
    expr += QStringLiteral(" WHEN \"period\" = '%1' THEN color_rgba(%2,%3,%4,%5)")
                .arg(p.label)
                .arg(c.red())
                .arg(c.green())
                .arg(c.blue())
                .arg(alpha);
  }
  expr += QStringLiteral(" ELSE color_rgba(100,116,139,%1) END").arg(alpha);
  return expr;
}

bool FeaturePresets::applyAttributes(QgsFeature* feature, const QString& kindLabel,
                                     const QString& periodLabel) {
  if (!feature) return false;
  const QgsFields fields = feature->fields();
  bool any = false;
  const int ik = fields.indexOf(QStringLiteral("kind"));
  if (ik >= 0) {
    feature->setAttribute(ik, kindLabel);
    any = true;
  }
  const int ip = fields.indexOf(QStringLiteral("period"));
  if (ip >= 0) {
    feature->setAttribute(ip, periodLabel);
    any = true;
  }
  return any;
}

void FeaturePresets::applyPeriodColor(QgsSymbol* symbol, int geomType) const {
  if (!symbol || symbol->symbolLayerCount() <= 0) return;
  QgsSymbolLayer* sl = symbol->symbolLayer(0);
  if (!sl) return;
  const QString fillExpr = periodColorExpression(geomType == static_cast<int>(Qgis::GeometryType::Polygon) ? 150 : 230);
  const QString strokeExpr = periodColorExpression(255);
  if (geomType == static_cast<int>(Qgis::GeometryType::Line)) {
    sl->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor,
                               QgsProperty::fromExpression(strokeExpr));
  } else {
    sl->setDataDefinedProperty(QgsSymbolLayer::Property::FillColor,
                               QgsProperty::fromExpression(fillExpr));
    sl->setDataDefinedProperty(QgsSymbolLayer::Property::StrokeColor,
                               QgsProperty::fromExpression(strokeExpr));
  }
}

QgsSymbol* FeaturePresets::symbolFor(int geomType, const Kind& kind) const {
  if (geomType == static_cast<int>(Qgis::GeometryType::Polygon)) {
    auto fs = QgsFillSymbol::createSimple({
        {QStringLiteral("color"), QStringLiteral("#64748B80")},
        {QStringLiteral("style"), kind.pattern},
        {QStringLiteral("outline_color"), QStringLiteral("#334155")},
        {QStringLiteral("outline_width"), QStringLiteral("1.1")},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
    });
    return fs.release();
  }
  if (geomType == static_cast<int>(Qgis::GeometryType::Line)) {
    QString pen = QStringLiteral("solid");
    if (kind.pattern == QLatin1String("horizontal") || kind.pattern == QLatin1String("vertical"))
      pen = QStringLiteral("dash");
    else if (kind.pattern == QLatin1String("dense6") || kind.pattern == QLatin1String("bdiagonal"))
      pen = QStringLiteral("dot");
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), QStringLiteral("#334155")},
        {QStringLiteral("line_width"), QStringLiteral("1.6")},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"), pen},
    });
    return ls.release();
  }
  QString marker = kind.marker;
  if (marker == QLatin1String("line")) marker = QStringLiteral("line");
  auto ms = QgsMarkerSymbol::createSimple({
      {QStringLiteral("name"), marker},
      {QStringLiteral("color"), QStringLiteral("#64748B")},
      {QStringLiteral("outline_color"), QStringLiteral("#1E293B")},
      {QStringLiteral("outline_width"), QStringLiteral("0.5")},
      {QStringLiteral("size"), QStringLiteral("3.6")},
      {QStringLiteral("size_unit"), QStringLiteral("MM")},
  });
  return ms.release();
}

bool FeaturePresets::applyRenderer(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (!ensureLoaded()) return false;
  if (layer->fields().indexOf(QStringLiteral("kind")) < 0) return false;
  const int gt = static_cast<int>(layer->geometryType());
  if (gt != static_cast<int>(Qgis::GeometryType::Polygon)
      && gt != static_cast<int>(Qgis::GeometryType::Line)
      && gt != static_cast<int>(Qgis::GeometryType::Point))
    return false;

  QgsCategoryList cats;
  for (const Kind& k : m_kinds) {
    QgsSymbol* sym = symbolFor(gt, k);
    if (!sym) continue;
    applyPeriodColor(sym, gt);
    cats.append(QgsRendererCategory(k.label, sym, k.label));
  }
  if (cats.isEmpty()) return false;
  auto* renderer = new QgsCategorizedSymbolRenderer(QStringLiteral("kind"), cats);
  layer->setRenderer(renderer);
  layer->triggerRepaint();
  return true;
}
