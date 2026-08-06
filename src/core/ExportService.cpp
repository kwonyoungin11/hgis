#include "ExportService.h"
#include "LayoutService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QStringConverter>
#include <QCryptographicHash>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransformcontext.h>

static QgsVectorLayer* layerByName(QgsProject* p, const QString& name) {
  if (!p) return nullptr;
  const auto layers = p->mapLayersByName(name);
  if (layers.isEmpty()) return nullptr;
  return qobject_cast<QgsVectorLayer*>(layers.first());
}

bool ExportService::writeSha256Manifest(const QString& dir, QString* errorOut) {
  QDir d(dir);
  if (!d.exists()) {
    if (errorOut) *errorOut = QStringLiteral("dir missing");
    return false;
  }
  const QString manPath = d.filePath(QStringLiteral("MANIFEST.sha256"));
  QFile man(manPath);
  if (!man.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("cannot write manifest");
    return false;
  }
  QTextStream ts(&man);
  ts.setEncoding(QStringConverter::Utf8);
  const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  for (const QFileInfo& fi : files) {
    if (fi.fileName() == QLatin1String("MANIFEST.sha256")) continue;
    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly)) continue;
    const QByteArray hash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
    ts << QString::fromLatin1(hash) << "  " << fi.fileName() << "\n";
  }
  return true;
}

QString ExportService::writePdfViaLayout(QgsProject* project, const QString& layoutName,
                                         const QString& outPath, QString* errorOut) {
  return LayoutService::exportLayoutPdf(project, layoutName, outPath, errorOut);
}

QString ExportService::exportSubmissionPackage(QgsProject* project,
                                               const QString& outDir,
                                               const QString& encoding,
                                               const QString& checklistSummary,
                                               bool blockOnError,
                                               bool hasChecklistErrors,
                                               QString* errorOut) {
  if (blockOnError && hasChecklistErrors) {
    if (errorOut) *errorOut = QStringLiteral("Checklist errors remain; export blocked.");
    return {};
  }
  QDir dir(outDir);
  if (!dir.exists() && !QDir().mkpath(outDir)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot create output folder");
    return {};
  }

  const QString enc = (encoding.compare(QStringLiteral("EUC-KR"), Qt::CaseInsensitive) == 0
                       || encoding.compare(QStringLiteral("CP949"), Qt::CaseInsensitive) == 0)
                          ? QStringLiteral("CP949")
                          : QStringLiteral("UTF-8");

  if (project) {
    const QStringList names = {
      QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
      QStringLiteral("feature_line"), QStringLiteral("control_points"),
      QStringLiteral("section_line")
    };
    for (const QString& n : names) {
      QgsVectorLayer* vl = layerByName(project, n);
      if (!vl || vl->featureCount() <= 0) continue;
      const QString shp = dir.filePath(n + QStringLiteral(".shp"));
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("ESRI Shapefile");
      opts.fileEncoding = enc;
      QString errMsg, newFn, newLayer;
      const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
          vl, shp, project->transformContext(), opts, &errMsg, &newFn, &newLayer);
      if (we != QgsVectorFileWriter::NoError) {
        if (errorOut) *errorOut = errMsg.isEmpty() ? QStringLiteral("SHP failed: %1").arg(n) : errMsg;
        return {};
      }
    }
  }

  const QString readme = dir.filePath(QStringLiteral("README_submit.txt"));
  QFile f(readme);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write README");
    return {};
  }
  QTextStream ts(&f);
  ts.setEncoding(QStringConverter::Utf8);
  ts << "KA-HGIS submission package\n";
  ts << "created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  ts << "crs: EPSG:5179\n";
  ts << "shp_encoding: " << enc << "\n\n";
  ts << "checklist:\n" << checklistSummary << "\n";
  ts << "\nSee MANIFEST.sha256 for file hashes.\n";
  f.close();

  QFile encf(dir.filePath(QStringLiteral("encoding.txt")));
  if (encf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    encf.write(enc.toUtf8());
    encf.write("\n");
  }

  QString merr;
  if (!writeSha256Manifest(outDir, &merr)) {
    if (errorOut) *errorOut = merr;
    return {};
  }
  return outDir;
}
