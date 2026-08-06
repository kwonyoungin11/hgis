#pragma once
#include <QString>
#include <QVariantMap>
#include <qgsfeature.h>
#include <qgsgeometry.h>

class QgsVectorLayer;

struct FeatureWriteResult {
  bool ok = false;
  QString errorKo;
  QgsFeatureId featureId = FID_NULL;
};

class FeatureWriteService {
public:
  static bool isAcceptablePolygon(const QgsGeometry& geom, QString* errorKo = nullptr);
  static bool isAcceptableLine(const QgsGeometry& geom, QString* errorKo = nullptr);
  static bool isAcceptablePoint(const QgsGeometry& geom, QString* errorKo = nullptr);

  // Validates geometry for layer type, required attrs (kind/period for feature_*), adds feature.
  // Layer must already be editable (or will startEditing). Does NOT commit.
  static FeatureWriteResult addFeature(QgsVectorLayer* layer,
                                       const QgsGeometry& geom,
                                       const QVariantMap& attributes = {});
};
