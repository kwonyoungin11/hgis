#include "SurveyStorage.h"
#include "LayerOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

#include <qgis.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

#include <gdal.h>
#include <cpl_error.h>
#include <ogr_api.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// GeoPackage 테이블 이름으로 안전한 형태. 한글은 그대로 둔다(GPKG는 UTF-8 식별자 허용).
QString sanitizeLayerName(const QString& name) {
  QString out = name;
  out.replace(QRegularExpression(QStringLiteral("[^\\w\\s가-힣._-]")), QStringLiteral("_"));
  out.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
  out = out.trimmed();
  if (out.isEmpty()) out = QStringLiteral("layer");
  // 숫자로 시작하는 테이블 이름은 일부 SQL 경로에서 다루기 번거롭다.
  if (out.at(0).isDigit()) out.prepend(QLatin1Char('L'));
  return out.left(60);
}

// 이 레이어가 이미 조사 .gpkg 안에 있는가. source는 "경로|layername=..." 형태다.
bool livesInGpkg(const QgsVectorLayer* layer, const QString& gpkgPath) {
  if (!layer) return false;
  const QString src = layer->source();
  const QString file = src.section(QLatin1Char('|'), 0, 0);
  if (file.isEmpty()) return false;
  return QFileInfo(file).absoluteFilePath().compare(QFileInfo(gpkgPath).absoluteFilePath(),
                                                    Qt::CaseInsensitive) == 0;
}

QSet<QString> existingGpkgLayerNames(const QString& gpkgPath) {
  QSet<QString> names;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                               nullptr);
  if (!ds) return names;
  const int n = GDALDatasetGetLayerCount(ds);
  for (int i = 0; i < n; ++i) {
    OGRLayerH l = GDALDatasetGetLayer(ds, i);
    if (l) names.insert(QString::fromUtf8(OGR_L_GetName(l)));
  }
  GDALClose(ds);
  return names;
}

bool readProjectPlain(QgsProject* project, const QString& uri) {
  return project->read(uri, Qgis::ProjectReadFlag::DontLoadLayouts);
}

// SEH를 쓰는 함수에는 소멸자가 필요한 지역 객체를 둘 수 없다(C2712). 그래서 감시만
// 하는 얇은 함수로 분리한다.
bool readGuarded(QgsProject* project, const QString& uri, bool* crashed) {
#ifdef Q_OS_WIN
  __try {
    return readProjectPlain(project, uri);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    *crashed = true;
    return false;
  }
#else
  return readProjectPlain(project, uri);
#endif
}

}  // namespace

namespace SurveyStorage {

QString projectUri(const QString& gpkgPath) {
  if (gpkgPath.isEmpty()) return {};
  return QStringLiteral("geopackage:%1?projectName=survey")
      .arg(QFileInfo(gpkgPath).absoluteFilePath());
}

bool hasEmbeddedProject(const QString& gpkgPath) {
  if (gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return false;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                               nullptr);
  if (!ds) return false;
  // QGIS는 내장 프로젝트를 qgis_projects 테이블에 넣는다. 지오메트리가 없는 테이블이라
  // 레이어 목록에는 안 잡히므로 SQL로 직접 확인한다.
  OGRLayerH res = GDALDatasetExecuteSQL(
      ds, "SELECT name FROM sqlite_master WHERE type='table' AND name='qgis_projects'", nullptr,
      nullptr);
  bool found = false;
  if (res) {
    found = OGR_L_GetFeatureCount(res, 1) > 0;
    GDALDatasetReleaseResultSet(ds, res);
  }
  GDALClose(ds);
  return found;
}

bool validateForOpen(const QString& gpkgPath, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const char* drivers[] = {"GPKG", nullptr};
  GDALDatasetH dataset = GDALOpenEx(gpkgPath.toUtf8().constData(),
      GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!dataset) {
    if (errorOut) *errorOut = QStringLiteral("읽을 수 있는 GeoPackage 조사 파일이 아닙니다.");
    return false;
  }
  OGRLayerH result = GDALDatasetExecuteSQL(dataset, "PRAGMA quick_check", nullptr, nullptr);
  OGRFeatureH check = result ? OGR_L_GetNextFeature(result) : nullptr;
  const bool valid = check && QString::fromUtf8(OGR_F_GetFieldAsString(check, 0)) == QLatin1String("ok");
  if (check) OGR_F_Destroy(check);
  if (result) GDALDatasetReleaseResultSet(dataset, result);
  GDALClose(dataset);
  if (!valid && errorOut)
    *errorOut = QStringLiteral("조사 파일의 무결성을 확인하지 못했습니다. 현재 작업은 유지됩니다.");
  return valid;
}

bool copySurvey(const QString& sourceGpkg, const QString& targetGpkg, QString* errorOut) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return false;
  };
  const QFileInfo source(sourceGpkg), target(targetGpkg);
  if (sourceGpkg.isEmpty() || targetGpkg.isEmpty() || !source.isFile())
    return fail(QStringLiteral("복사할 조사 파일 또는 저장 경로가 없습니다."));
  if (source.canonicalFilePath().compare(target.canonicalFilePath(), Qt::CaseInsensitive) == 0 ||
      source.absoluteFilePath().compare(target.absoluteFilePath(), Qt::CaseInsensitive) == 0)
    return true;
  const auto targetHasJournal = [&target]() {
    for (const QString& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")})
      if (QFileInfo::exists(target.absoluteFilePath() + suffix)) return true;
    return false;
  };
  if (targetHasJournal())
    return fail(QStringLiteral("저장 대상 조사 파일이 사용 중입니다. 해당 파일을 닫고 다시 저장하세요."));
  QTemporaryDir temporary(target.dir().filePath(QStringLiteral(".ka-survey-copy-XXXXXX")));
  if (!temporary.isValid()) return fail(QStringLiteral("저장 폴더에 임시 사본을 만들 수 없습니다."));
  const QString snapshot = temporary.filePath(QStringLiteral("survey.gpkg"));
  GDALDatasetH dataset = GDALOpenEx(source.absoluteFilePath().toUtf8().constData(), GDAL_OF_VECTOR,
                                    nullptr, nullptr, nullptr);
  if (!dataset) return fail(QStringLiteral("원본 조사 파일을 읽을 수 없습니다."));
  // SQLite VACUUM INTO takes a consistent read snapshot, including committed WAL,
  // without modifying the source or relying on a byte copy of its main file.
  QString escaped = snapshot;
  escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
  const QByteArray sql = QStringLiteral("VACUUM INTO '%1'").arg(escaped).toUtf8();
  CPLErrorReset();
  OGRLayerH result = GDALDatasetExecuteSQL(dataset, sql.constData(), nullptr, nullptr);
  const bool copied = CPLGetLastErrorType() < CE_Failure;
  const QString copyError = QString::fromUtf8(CPLGetLastErrorMsg());
  if (result) GDALDatasetReleaseResultSet(dataset, result);
  GDALClose(dataset);
  if (!copied || QFileInfo(snapshot).size() <= 0)
    return fail(QStringLiteral("조사 파일 사본을 만들지 못했습니다: %1").arg(copyError));
  QFile input(snapshot);
  QSaveFile output(target.absoluteFilePath());
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return fail(QStringLiteral("저장 대상에 쓸 수 없습니다: %1").arg(output.errorString()));
  while (!input.atEnd()) {
    const QByteArray data = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(data) != data.size())
      return fail(QStringLiteral("조사 파일 사본을 기록하지 못했습니다."));
  }
  if (targetHasJournal())
    return fail(QStringLiteral("저장 도중 대상 조사 파일이 열려 저장을 멈췄습니다."));
  if (!output.commit()) return fail(QStringLiteral("저장 파일을 교체하지 못했습니다: %1").arg(output.errorString()));
  return true;
}

AbsorbResult absorbExternalVectors(QgsProject* project, const QString& gpkgPath) {
  AbsorbResult r;
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return r;

  QSet<QString> used = existingGpkgLayerNames(gpkgPath);
  const QList<QgsMapLayer*> layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml || !ml->isValid()) continue;
    if (qobject_cast<QgsRasterLayer*>(ml)) {
      // 래스터(스크린샷·항공사진)는 아직 바깥에 둔다. .gpkg 타일로 굽는 것은 되돌릴 수
      // 없는 변환이라 사용자가 원할 때만 해야 한다.
      const QString src = ml->source();
      if (QFileInfo::exists(src.section(QLatin1Char('|'), 0, 0)))
        r.skippedRaster << ml->name();
      continue;
    }
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl) continue;
    const QString provider = vl->providerType().toLower();
    // 파일에서 온 것(ogr)과 메모리 레이어만 대상. xyz/wms 배경지도는 파일이 아니다.
    if (provider != QLatin1String("ogr") && provider != QLatin1String("memory")) continue;
    if (provider == QLatin1String("ogr") && livesInGpkg(vl, gpkgPath)) continue;

    QString target = sanitizeLayerName(vl->name());
    int suffix = 2;
    while (used.contains(target)) target = sanitizeLayerName(vl->name()) + QStringLiteral("_%1").arg(suffix++);

    QgsVectorFileWriter::SaveVectorOptions opt;
    opt.driverName = QStringLiteral("GPKG");
    opt.layerName = target;
    opt.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    opt.fileEncoding = QStringLiteral("UTF-8");
    QString err;
    QString newFile;
    QString newLayer;
    const QgsVectorFileWriter::WriterError code = QgsVectorFileWriter::writeAsVectorFormatV3(
        vl, gpkgPath, project->transformContext(), opt, &err, &newFile, &newLayer);
    if (code != QgsVectorFileWriter::NoError) {
      r.failed << vl->name();
      continue;
    }
    used.insert(target);
    // 스타일·라벨은 레이어 객체에 남아 있으므로 데이터소스만 사본으로 돌린다.
    const QString name = vl->name();
    vl->setDataSource(QStringLiteral("%1|layername=%2").arg(gpkgPath, target), name,
                      QStringLiteral("ogr"));
    if (vl->isValid())
      r.imported << name;
    else
      r.failed << name;
  }
  return r;
}

bool writeEmbedded(QgsProject* project, const QString& gpkgPath, QString* errorOut) {
  if (!project || gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("저장 경로가 없습니다.");
    return false;
  }
  if (!QFileInfo::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일이 없습니다: %1").arg(gpkgPath);
    return false;
  }
  const QString abs = QFileInfo(gpkgPath).absoluteFilePath();
  project->setFileName(abs);
  project->setPresetHomePath(QFileInfo(abs).absolutePath());
  const QString uri = projectUri(gpkgPath);
  if (!project->write(uri)) {
    if (errorOut) *errorOut = project->error();
    return false;
  }
  LayerOps::saveGpkgDefaultStyles(project, abs);
  return true;
}

bool readEmbedded(QgsProject* project, const QString& gpkgPath, bool* crashedOut,
                  QString* errorOut) {
  if (crashedOut) *crashedOut = false;
  if (!project || gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일이 없습니다.");
    return false;
  }
  const QString abs = QFileInfo(gpkgPath).absoluteFilePath();
  project->setFileName(abs);
  project->setPresetHomePath(QFileInfo(abs).absolutePath());
  const QString uri = projectUri(gpkgPath);
  bool crashed = false;
  const bool ok = readGuarded(project, uri, &crashed);
  if (crashedOut) *crashedOut = crashed;
  if (!ok && errorOut)
    *errorOut = crashed ? QStringLiteral("조사 파일 안의 작업공간을 읽다 오류가 났습니다.")
                        : project->error();
  return ok;
}

}  // namespace SurveyStorage
