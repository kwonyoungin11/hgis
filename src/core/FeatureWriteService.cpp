#include "FeatureWriteService.h"
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>
#include <qgsgeometry.h>

static int ringVertexCount(const QgsGeometry& geom) {
  if (geom.isEmpty()) return 0;
  const QgsPolygonXY polys = geom.asPolygon();
  if (!polys.isEmpty()) return polys.first().size();
  const auto multi = geom.asMultiPolygon();
  if (!multi.isEmpty() && !multi.first().isEmpty()) return multi.first().first().size();
  return geom.constGet() ? int(geom.constGet()->nCoordinates()) : 0;
}

bool FeatureWriteService::isAcceptablePolygon(const QgsGeometry& geom, QString* errorKo) {
  if (geom.isEmpty()) {
    if (errorKo) *errorKo = QStringLiteral("폴리곤 도형이 비어 있습니다. 점 3개 이상 찍어 주세요.");
    return false;
  }
  const Qgis::GeometryType gt = QgsWkbTypes::geometryType(geom.wkbType());
  if (gt != Qgis::GeometryType::Polygon) {
    if (errorKo) *errorKo = QStringLiteral("조사구역·유구면은 폴리곤(면)이어야 합니다. 점/선만으로는 안 됩니다.");
    return false;
  }
  const int n = ringVertexCount(geom);
  if (n < 4) {
    if (errorKo) *errorKo = QStringLiteral("면은 꼭짓점 3개 이상 필요합니다. (닫힌 링 기준 4좌표)");
    return false;
  }
  QgsGeometry g = geom;
  if (!g.isGeosValid()) {
    g = g.makeValid();
    if (g.isEmpty()) {
      if (errorKo) *errorKo = QStringLiteral("유효하지 않은 면 도형입니다. 선을 꼬이지 않게 다시 그려 주세요.");
      return false;
    }
  }
  if (g.area() <= 0) {
    if (errorKo) *errorKo = QStringLiteral("면적이 0입니다. 점 3개 이상으로 크게 그려 주세요.");
    return false;
  }
  return true;
}

bool FeatureWriteService::isAcceptableLine(const QgsGeometry& geom, QString* errorKo) {
  if (geom.isEmpty()) {
    if (errorKo) *errorKo = QStringLiteral("선 도형이 비어 있습니다. 점 2개 이상 찍어 주세요.");
    return false;
  }
  const Qgis::GeometryType gt = QgsWkbTypes::geometryType(geom.wkbType());
  if (gt != Qgis::GeometryType::Line) {
    if (errorKo) *errorKo = QStringLiteral("유구/단면 선은 라인(선)이어야 합니다.");
    return false;
  }
  const int n = geom.constGet() ? int(geom.constGet()->nCoordinates()) : 0;
  if (n < 2) {
    if (errorKo) *errorKo = QStringLiteral("선은 점 2개 이상 필요합니다.");
    return false;
  }
  if (geom.length() <= 0) {
    if (errorKo) *errorKo = QStringLiteral("선 길이가 0입니다.");
    return false;
  }
  return true;
}

bool FeatureWriteService::isAcceptablePoint(const QgsGeometry& geom, QString* errorKo) {
  if (geom.isEmpty()) {
    if (errorKo) *errorKo = QStringLiteral("점 도형이 비어 있습니다.");
    return false;
  }
  if (QgsWkbTypes::geometryType(geom.wkbType()) != Qgis::GeometryType::Point) {
    if (errorKo) *errorKo = QStringLiteral("기준점은 포인트여야 합니다.");
    return false;
  }
  return true;
}

FeatureWriteResult FeatureWriteService::addFeature(QgsVectorLayer* layer,
                                                   const QgsGeometry& geom,
                                                   const QVariantMap& attributes) {
  FeatureWriteResult r;
  if (!layer || !layer->isValid()) {
    r.errorKo = QStringLiteral("레이어가 없습니다. 먼저 「새 조사」를 만드세요.");
    return r;
  }
  if (!layer->isEditable() && !layer->startEditing()) {
    r.errorKo = QStringLiteral("레이어를 편집할 수 없습니다: %1").arg(layer->name());
    return r;
  }

  QString gerr;
  const Qgis::GeometryType lgt = layer->geometryType();
  QgsGeometry useGeom = geom;
  if (lgt == Qgis::GeometryType::Polygon) {
    if (!isAcceptablePolygon(useGeom, &gerr)) {
      r.errorKo = gerr;
      return r;
    }
    if (!useGeom.isGeosValid()) {
      const QgsGeometry fixed = useGeom.makeValid();
      if (!fixed.isEmpty()) useGeom = fixed;
    }
  } else if (lgt == Qgis::GeometryType::Line) {
    if (!isAcceptableLine(useGeom, &gerr)) {
      r.errorKo = gerr;
      return r;
    }
  } else if (lgt == Qgis::GeometryType::Point) {
    if (!isAcceptablePoint(useGeom, &gerr)) {
      r.errorKo = gerr;
      return r;
    }
  } else {
    r.errorKo = QStringLiteral("지원하지 않는 레이어 기하 유형입니다.");
    return r;
  }

  const QString name = layer->name();
  if (name == QLatin1String("feature_poly") || name == QLatin1String("feature_line")) {
    const QString kind = attributes.value(QStringLiteral("kind")).toString().trimmed();
    const QString period = attributes.value(QStringLiteral("period")).toString().trimmed();
    if (kind.isEmpty() || period.isEmpty()) {
      r.errorKo = QStringLiteral("유구는 「종류」와 「시대」가 필수입니다. (범례·보고서용)");
      return r;
    }
  }

  QgsFeature feat(layer->fields());
  feat.setGeometry(useGeom);
  for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
    const int idx = layer->fields().indexOf(it.key());
    if (idx >= 0) feat.setAttribute(idx, it.value());
  }

  if (!layer->addFeature(feat)) {
    r.errorKo = QStringLiteral("도형 저장 실패. 다시 그려 주세요.");
    return r;
  }
  r.ok = true;
  r.featureId = feat.id();
  return r;
}
