#include "SurveyProjectFactory.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <qgscoordinatereferencesystem.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgscoordinatetransformcontext.h>

QString SurveyProjectFactory::createNewSurvey(const QString& directory,
                                              const QString& surveyName,
                                              QString* errorOut,
                                              const QString& workCrsAuthId) {
  QDir dir(directory);
  if (!dir.exists() && !QDir().mkpath(directory)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot create folder");
    return {};
  }
  QString safe = surveyName.trimmed();
  if (safe.isEmpty()) safe = QStringLiteral("survey");
  safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));

  const QString gpkgPath = dir.filePath(safe + QStringLiteral(".gpkg"));
  if (QFile::exists(gpkgPath) && !QFile::remove(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot overwrite existing GPKG file (file may be locked): %1").arg(gpkgPath);
    return {};
  }

  QString crsId = workCrsAuthId.trimmed();
  if (crsId.isEmpty()) crsId = QString::fromUtf8(defaultWorkCrsAuthId());
  QgsCoordinateReferenceSystem crs(crsId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid work CRS: %1").arg(crsId);
    return {};
  }
  QgsCoordinateTransformContext transformContext;

  struct LayerDef { const char* name; const char* geom; };
  const LayerDef defs[] = {
    {"survey_area", "Polygon"},
    {"feature_poly", "Polygon"},
    {"feature_line", "LineString"},
    {"section_line", "LineString"},
    {"control_points", "Point"},
    {"artifact_point", "Point"},
    {"trial_trench", "Polygon"},
  };

  bool first = true;
  for (const LayerDef& d : defs) {
    const QString uri = QStringLiteral("%1?crs=%2").arg(QString::fromUtf8(d.geom), crs.authid());
    QgsVectorLayer mem(uri, QString::fromUtf8(d.name), QStringLiteral("memory"));
    if (!mem.isValid()) {
      if (errorOut) *errorOut = QStringLiteral("memory layer failed: %1").arg(d.name);
      return {};
    }
    QgsFields fields;
    const QString n = QString::fromUtf8(d.name);
    if (n == QLatin1String("survey_area")) {
      fields.append(QgsField(QStringLiteral("survey_name"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("site_name"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    } else if (n == QLatin1String("feature_poly")) {
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("feature_no"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    } else if (n == QLatin1String("feature_line")) {
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    } else if (n == QLatin1String("section_line")) {
      fields.append(QgsField(QStringLiteral("section_id"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    } else if (n == QLatin1String("artifact_point")) {
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("artifact_no"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    } else if (n == QLatin1String("trial_trench")) {
      fields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("width"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("length"), QMetaType::Type::Double));
    } else {
      fields.append(QgsField(QStringLiteral("point_id"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("x"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("y"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("z"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("datum"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("ellipsoid"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("projection"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("origin"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("accuracy"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("accuracy_m"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("pdop"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("fix_type"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("pixel_x"), QMetaType::Type::Double));
      fields.append(QgsField(QStringLiteral("pixel_y"), QMetaType::Type::Double));
    }
    mem.dataProvider()->addAttributes(fields.toList());
    mem.updateFields();
    mem.setCrs(crs);

    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = n;
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = first
      ? QgsVectorFileWriter::CreateOrOverwriteFile
      : QgsVectorFileWriter::CreateOrOverwriteLayer;
    first = false;

    QString errorMessage;
    QString newFilename;
    QString newLayer;
    const auto err = QgsVectorFileWriter::writeAsVectorFormatV3(
      &mem, gpkgPath, transformContext, opts, &errorMessage, &newFilename, &newLayer);
    if (err != QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errorMessage.isEmpty()
        ? QStringLiteral("GPKG write failed: %1").arg(n) : errorMessage;
      return {};
    }
  }

  QgsProject proj;
  proj.setCrs(crs);
  proj.setTitle(safe);
  for (const LayerDef& d : defs) {
    const QString src = QStringLiteral("%1|layername=%2").arg(gpkgPath, QString::fromUtf8(d.name));
    auto* vl = new QgsVectorLayer(src, QString::fromUtf8(d.name), QStringLiteral("ogr"));
    if (vl->isValid()) proj.addMapLayer(vl);
    else delete vl;
  }
  const QString qgz = dir.filePath(safe + QStringLiteral(".qgz"));
  proj.write(qgz);
  return gpkgPath;
}

