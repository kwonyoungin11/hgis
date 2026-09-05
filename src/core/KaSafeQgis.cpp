#include "KaSafeQgis.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#include <qgsproject.h>
#include <qgsziputils.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static bool readProjectPlain(QgsProject* project, const QString& path) {
  return project->read(path);
}

static void clearProjectPlain(QgsProject* project) { project->clear(); }

bool kaSafeReadQgisProject(QgsProject* project, const QString& path) {
  if (!project || path.isEmpty())
    return false;
#ifdef Q_OS_WIN
  bool ok = false;
  __try {
    ok = readProjectPlain(project, path);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return ok;
#else
  return readProjectPlain(project, path);
#endif
}

bool kaSafeClearQgisProject(QgsProject* project) {
  if (!project)
    return false;
#ifdef Q_OS_WIN
  __try {
    clearProjectPlain(project);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
#else
  clearProjectPlain(project);
  return true;
#endif
}

static bool nameLooksSatellite(const QString& name) {
  return name.contains(QStringLiteral("위성")) ||
         name.contains(QStringLiteral("Satellite"), Qt::CaseInsensitive);
}

static int countSatelliteNamesInXml(const QString& xml) {
  int fromLayerName = 0;
  int fromTree = 0;
  const QRegularExpression layerRe(QStringLiteral("<layername>([^<]*)</layername>"),
                                   QRegularExpression::CaseInsensitiveOption);
  auto it = layerRe.globalMatch(xml);
  while (it.hasNext()) {
    if (nameLooksSatellite(it.next().captured(1).trimmed()))
      ++fromLayerName;
  }
  const QRegularExpression treeRe(
      QStringLiteral("layer-tree-layer[^>]*\\bname=\"([^\"]+)\""),
      QRegularExpression::CaseInsensitiveOption);
  auto treeIt = treeRe.globalMatch(xml);
  while (treeIt.hasNext()) {
    if (nameLooksSatellite(treeIt.next().captured(1).trimmed()))
      ++fromTree;
  }
  return qMax(fromLayerName, fromTree);
}

static QString readQgsXml(const QString& qgsPath) {
  QFile f(qgsPath);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}

int kaCountSatelliteLayersInQgisProjectFile(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return 0;
  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == QLatin1String("qgs")) {
    const QString xml = readQgsXml(path);
    if (xml.isEmpty())
      return -1;
    return countSatelliteNamesInXml(xml);
  }
  if (suffix != QLatin1String("qgz") && suffix != QLatin1String("zip"))
    return 0;

  QTemporaryDir dir;
  if (!dir.isValid())
    return -1;
  QStringList files;
  if (!QgsZipUtils::unzip(path, dir.path(), files, false))
    return -1;
  QString xml;
  for (const QString& f : files) {
    if (f.endsWith(QLatin1String(".qgs"), Qt::CaseInsensitive)) {
      xml = readQgsXml(f);
      if (!xml.isEmpty())
        break;
    }
  }
  if (xml.isEmpty()) {
    const QStringList found =
        QDir(dir.path()).entryList(QStringList{QStringLiteral("*.qgs")}, QDir::Files);
    if (!found.isEmpty())
      xml = readQgsXml(dir.filePath(found.first()));
  }
  if (xml.isEmpty())
    return -1;
  return countSatelliteNamesInXml(xml);
}

bool kaQgisProjectFileHasDuplicateSatellites(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return false;
  const int n = kaCountSatelliteLayersInQgisProjectFile(path);
  return n >= 2;
}

static QSet<QString> g_unsafeQgisProjects;

void kaMarkQgisProjectUnsafeToRead(const QString& path) {
  if (path.isEmpty())
    return;
  g_unsafeQgisProjects.insert(QFileInfo(path).absoluteFilePath());
}

bool kaQgisProjectFileIsUnsafeToRead(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path))
    return false;
  // Duplicate satellites are pruned after a successful read. Skipping the
  // whole .qgz here dropped every imported SHP (안동시 15장).
  return g_unsafeQgisProjects.contains(QFileInfo(path).absoluteFilePath());
}
