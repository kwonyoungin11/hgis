#include <stdexcept>
#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "core/KaSafeQgis.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class ReadLock {
 public:
  explicit ReadLock(const QString& path)
      : m_handle(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
  ~ReadLock() {
    if (valid()) CloseHandle(m_handle);
  }
  bool valid() const { return m_handle != INVALID_HANDLE_VALUE; }
  ReadLock(const ReadLock&) = delete;
  ReadLock& operator=(const ReadLock&) = delete;

 private:
  HANDLE m_handle;
};
#endif

static QByteArray fileHash(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) return {};
  return hash.result();
}

static bool writeBytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

static QStringList temporaryArtifacts(const QString& path) {
  return QDir(path).entryList({QStringLiteral(".ka-new-survey-*"), QStringLiteral("*ka-writing*")},
                              QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

class TestStorageSafety : public QObject {
  Q_OBJECT
 private slots:
  void newSurvey_existingFilesRemainByteIdentical() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("보존할조사"), &error,
                                                               QStringLiteral("EPSG:5187"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    const QString qgz = dir.filePath(QStringLiteral("보존할조사.qgz"));
    const auto gpkgBefore = fileHash(gpkg);
    const auto qgzBefore = fileHash(qgz);
    QVERIFY(!gpkgBefore.isEmpty() && !qgzBefore.isEmpty());
    const QString duplicate = SurveyProjectFactory::createNewSurvey(
        dir.path(), QStringLiteral("보존할조사"), &error, QStringLiteral("EPSG:5186"));
    QCOMPARE(fileHash(gpkg), gpkgBefore);
    QCOMPARE(fileHash(qgz), qgzBefore);
    QVERIFY(duplicate.isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_orphanWorkspaceIsNotOverwritten() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qgz = dir.filePath(QStringLiteral("보존할작업.qgz"));
    QVERIFY(writeBytes(qgz, QByteArray("existing workspace bytes")));
    const auto before = fileHash(qgz);
    QString error;
    const QString duplicate = SurveyProjectFactory::createNewSurvey(
        dir.path(), QStringLiteral("보존할작업"), &error, QStringLiteral("EPSG:5187"));
    QCOMPARE(fileHash(qgz), before);
    QVERIFY(duplicate.isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("보존할작업.gpkg"))));
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_invalidCrsLeavesNoPartialFiles() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY(SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("실패한조사"), &error,
                                                  QStringLiteral("EPSG:999999999"))
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
  }

  void newSurvey_workspaceCollisionLeavesNoPartialGpkg() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qgz = dir.filePath(QStringLiteral("막힌조사.qgz"));
    QVERIFY(QDir().mkpath(qgz));
    QVERIFY(writeBytes(QDir(qgz).filePath(QStringLiteral("keep.txt")), QByteArray("keep")));
    QString error;
    QVERIFY(SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("막힌조사"), &error,
                                                  QStringLiteral("EPSG:5187"))
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(QDir(qgz).filePath(QStringLiteral("keep.txt"))));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("막힌조사.gpkg"))));
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_schemaExistsButWorkspaceIsEmpty_data() {
    QTest::addColumn<QString>("crs");
    QTest::newRow("central") << QStringLiteral("EPSG:5186");
    QTest::newRow("east") << QStringLiteral("EPSG:5187");
  }

  void newSurvey_schemaExistsButWorkspaceIsEmpty() {
    QFETCH(QString, crs);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("빈조사"), &error, crs);
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QVERIFY2(SurveyStorage::validateForOpen(gpkg, &error), qPrintable(error));
    QgsVectorLayer survey(gpkg + QStringLiteral("|layername=survey_area"), QStringLiteral("schema"),
                          QStringLiteral("ogr"));
    QVERIFY(survey.isValid());
    QCOMPARE(survey.featureCount(), 0LL);
    QCOMPARE(survey.crs().authid(), crs);
    QgsProject workspace;
    QVERIFY(workspace.read(dir.filePath(QStringLiteral("빈조사.qgz"))));
    QCOMPARE(workspace.crs().authid(), crs);
    QCOMPARE(workspace.mapLayers().size(), 0);
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void atomicWorkspace_preservesRelativeSourceAndPreviousGeneration() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg =
        SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("왕복"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* vector = new QgsVectorLayer(gpkg + QStringLiteral("|layername=survey_area"), QStringLiteral("구역"),
                                      QStringLiteral("ogr"));
    QVERIFY(vector->isValid());
    project.addMapLayer(vector);
    const QString path = dir.filePath(QStringLiteral("작업 화면.qgz"));
    project.setTitle(QStringLiteral("첫 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setTitle(QStringLiteral("두 번째 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    QCOMPARE(fileHash(kaProjectBackupPath(path)), before);
    QCOMPARE(project.fileName(), QFileInfo(path).absoluteFilePath());
    QgsProject reopened;
    QVERIFY(reopened.read(path));
    QCOMPARE(reopened.title(), QStringLiteral("두 번째 저장"));
    QCOMPARE(reopened.mapLayers().size(), 1);
    QVERIFY(reopened.mapLayers().first()->isValid());
    QCOMPARE(QFileInfo(reopened.mapLayers().first()->source().section(QLatin1Char('|'), 0, 0)).canonicalFilePath(),
             QFileInfo(gpkg).canonicalFilePath());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void atomicWorkspace_lockedTargetPreservesFileAndUnsavedState() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("잠긴작업.qgz"));
    QgsProject project;
    project.setTitle(QStringLiteral("이전 내용"));
    QString error;
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setFileName(QStringLiteral("original-session.qgz"));
    project.setTitle(QStringLiteral("아직 저장하지 못한 내용"));
    project.setDirty(true);
    ReadLock lock(path);
    QVERIFY(lock.valid());
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fileHash(path), before);
    QCOMPARE(project.fileName(), QStringLiteral("original-session.qgz"));
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
#else
    QSKIP("Windows sharing-lock failure contract");
#endif
  }

  void atomicWorkspace_lockedBackupPreservesBothFiles() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("백업실패.qgz"));
    const QString backup = kaProjectBackupPath(path);
    QgsProject project;
    QString error;
    project.setTitle(QStringLiteral("첫 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    project.setTitle(QStringLiteral("두 번째 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path), backupBefore = fileHash(backup);
    project.setTitle(QStringLiteral("저장되지 않을 세 번째"));
    project.setDirty(true);
    ReadLock lock(backup);
    QVERIFY(lock.valid());
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QCOMPARE(fileHash(path), before);
    QCOMPARE(fileHash(backup), backupBefore);
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
#else
    QSKIP("Windows sharing-lock failure contract");
#endif
  }

  void atomicWorkspace_serializationExceptionKeepsFileAndState() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("예외복구.qgz"));
    QgsProject project;
    QString error;
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setFileName(QStringLiteral("preserve-session.qgz"));
    project.setDirty(true);
    connect(
        &project, &QgsProject::writeProject, &project,
        [](QDomDocument&) { throw std::runtime_error("synthetic serialization failure"); }, Qt::DirectConnection);
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fileHash(path), before);
    QCOMPARE(project.fileName(), QStringLiteral("preserve-session.qgz"));
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QTemporaryDir settings;
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  TestStorageSafety tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_storage_safety.moc"
