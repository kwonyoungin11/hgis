#include "ProjectStateBuilder.h"
#include <QJsonObject>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsgeometry.h>
#include <qgsfeatureiterator.h>
#include <qgsfeature.h>
#include <qgswkbtypes.h>
#include <qgslayoutmanager.h>

QJsonObject ProjectStateBuilder::empty() {
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 0);
  st.insert(QStringLiteral("control_points_count"), 0);
  st.insert(QStringLiteral("feature_poly_count"), 0);
  st.insert(QStringLiteral("feature_line_count"), 0);
  st.insert(QStringLiteral("project_crs_set"), false);
  st.insert(QStringLiteral("work_crs"), QString());
  st.insert(QStringLiteral("has_datum"), false);
  st.insert(QStringLiteral("has_ellipsoid"), false);
  st.insert(QStringLiteral("has_projection"), false);
  st.insert(QStringLiteral("has_origin"), false);
  st.insert(QStringLiteral("has_accuracy"), false);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("survey_is_polygon"), false);
  st.insert(QStringLiteral("features_within_survey"), true);
  st.insert(QStringLiteral("geometries_valid"), true);
  st.insert(QStringLiteral("layout_exists:site_location"), false);
  st.insert(QStringLiteral("layout_exists:feature_plan"), false);
  st.insert(QStringLiteral("layout_exists:feature_detail"), false);
  st.insert(QStringLiteral("layout_exists:section"), false);
  st.insert(QStringLiteral("layout_exists:survey_area_map"), false);
  return st;
}

static QgsVectorLayer* layerByName(QgsProject* p, const QString& name) {
  if (!p) return nullptr;
  const auto layers = p->mapLayersByName(name);
  if (layers.isEmpty()) return nullptr;
  return qobject_cast<QgsVectorLayer*>(layers.first());
}

QJsonObject ProjectStateBuilder::fromProject(QgsProject* project) {
  QJsonObject st = empty();
  if (!project) return st;

  st.insert(QStringLiteral("project_crs_set"), project->crs().isValid());
  st.insert(QStringLiteral("work_crs"), project->crs().authid());

  auto* sa = layerByName(project, QStringLiteral("survey_area"));
  auto* fp = layerByName(project, QStringLiteral("feature_poly"));
  auto* fl = layerByName(project, QStringLiteral("feature_line"));
  auto* cp = layerByName(project, QStringLiteral("control_points"));

  const int saCount = sa ? int(sa->featureCount()) : 0;
  const int fpCount = fp ? int(fp->featureCount()) : 0;
  const int flCount = fl ? int(fl->featureCount()) : 0;
  const int cpCount = cp ? int(cp->featureCount()) : 0;
  st.insert(QStringLiteral("survey_area_count"), saCount);
  st.insert(QStringLiteral("feature_poly_count"), fpCount);
  st.insert(QStringLiteral("feature_line_count"), flCount);
  st.insert(QStringLiteral("control_points_count"), cpCount);

  bool surveyPoly = false;
  bool geosValid = true;
  QgsRectangle surveyExtent;
  bool hasSurveyExt = false;
  if (sa && saCount > 0) {
    QgsFeatureIterator it = sa->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QgsGeometry g = f.geometry();
      if (g.isNull()) continue;
      if (!g.isGeosValid()) geosValid = false;
      const Qgis::GeometryType gt = QgsWkbTypes::geometryType(g.wkbType());
      if (gt == Qgis::GeometryType::Polygon) surveyPoly = true;
      if (!hasSurveyExt) { surveyExtent = g.boundingBox(); hasSurveyExt = true; }
      else surveyExtent.combineExtentWith(g.boundingBox());
    }
  }
  st.insert(QStringLiteral("survey_is_polygon"), surveyPoly || saCount == 0);
  st.insert(QStringLiteral("has_abstract_marker"), saCount > 0 && !surveyPoly);

  bool hasKindPeriod = fpCount == 0;
  if (fp && fpCount > 0) {
    QgsFeatureIterator it = fp->getFeatures();
    QgsFeature f;
    hasKindPeriod = true;
    while (it.nextFeature(f)) {
      const QString kind = f.attribute(QStringLiteral("kind")).toString().trimmed();
      const QString period = f.attribute(QStringLiteral("period")).toString().trimmed();
      if (kind.isEmpty() || period.isEmpty()) hasKindPeriod = false;
      const QgsGeometry g = f.geometry();
      if (!g.isNull() && !g.isGeosValid()) geosValid = false;
      if (hasSurveyExt && !g.isNull()) {
        if (!surveyExtent.contains(g.boundingBox()) && !surveyExtent.intersects(g.boundingBox())) {
          st.insert(QStringLiteral("features_within_survey"), false);
        }
      }
    }
  }
  st.insert(QStringLiteral("has_kind_period"), hasKindPeriod);
  st.insert(QStringLiteral("geometries_valid"), geosValid);

  bool d=false,e=false,p=false,o=false,a=false;
  if (cp && cpCount > 0) {
    QgsFeatureIterator it = cp->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.attribute(QStringLiteral("datum")).toString().trimmed().isEmpty()) d = true;
      if (!f.attribute(QStringLiteral("ellipsoid")).toString().trimmed().isEmpty()) e = true;
      if (!f.attribute(QStringLiteral("projection")).toString().trimmed().isEmpty()) p = true;
      if (!f.attribute(QStringLiteral("origin")).toString().trimmed().isEmpty()) o = true;
      const QString acc = f.attribute(QStringLiteral("accuracy_m")).toString().trimmed();
      const QString acc2 = f.attribute(QStringLiteral("accuracy")).toString().trimmed();
      if (!acc.isEmpty() || !acc2.isEmpty()) a = true;
    }
  }
  st.insert(QStringLiteral("has_datum"), d);
  st.insert(QStringLiteral("has_ellipsoid"), e);
  st.insert(QStringLiteral("has_projection"), p);
  st.insert(QStringLiteral("has_origin"), o || cpCount == 0);
  st.insert(QStringLiteral("has_accuracy"), a || cpCount == 0);

  // layouts
  auto markLayout = [&](const QString& key, const QString& name) {
    const bool found = project->layoutManager() && project->layoutManager()->layoutByName(name) != nullptr;
    st.insert(key, found);
  };
  markLayout(QStringLiteral("layout_exists:site_location"), QStringLiteral("site_location"));
  markLayout(QStringLiteral("layout_exists:feature_plan"), QStringLiteral("feature_plan"));
  markLayout(QStringLiteral("layout_exists:feature_detail"), QStringLiteral("feature_detail"));
  markLayout(QStringLiteral("layout_exists:section"), QStringLiteral("section"));
  markLayout(QStringLiteral("layout_exists:survey_area_map"), QStringLiteral("survey_area_map"));

  return st;
}

