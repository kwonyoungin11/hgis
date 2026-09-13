#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <cpl_conv.h>
#include <optional>
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgsproject.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

#include "core/HeritageImport.h"
#include "core/HeritageSiteLegend.h"
#include "core/HeritageStyle.h"

namespace {
bool writeZip(const QString& path, const QList<QPair<QString, QByteArray>>& entries) {
  void* archive = CPLCreateZip(path.toUtf8().constData(), nullptr);
  if (!archive) return false;
  bool ok = true;
  for (const auto& entry : entries) {
    if (CPLCreateFileInZip(archive, entry.first.toUtf8().constData(), nullptr) != CE_None) {
      ok = false; break;
    }
    ok = CPLWriteFileInZip(archive, entry.second.constData(), static_cast<int>(entry.second.size())) == CE_None;
    ok = CPLCloseFileInZip(archive) == CE_None && ok;
    if (!ok) break;
  }
  return CPLCloseZip(archive) == CE_None && ok;
}
QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

// 실제로 인트라넷에서 받은 ZIP 으로 **적재까지** 되는지 본다.
// 사이트에 붙지 않고 확인할 수 있어야 한다 — 받는 쪽이 막혀도 올리는 쪽은 굳어 있어야 하기 때문이다.
//
// 표본은 build/qa/heritage-sample/지정유산.zip (실제 내려받은 파일).
// 없으면 건너뛴다. 리포에 자료를 넣지 않는다(배포 금지).
class HeritageImportTest : public QObject {
  Q_OBJECT

  static QString samplePath() {
    return qEnvironmentVariable("KA_HERITAGE_SAMPLE_ZIP", "build/qa/heritage-sample/지정유산.zip");
  }

private slots:
  void receivedIncompleteZipRequestsRetry() {
    const QString path = qEnvironmentVariable("KA_HERITAGE_INCOMPLETE_ZIP");
    if (path.isEmpty()) QSKIP("Local received ZIP inspection is opt-in.");
    QVERIFY(QFileInfo(path).isFile());
    const QByteArray before = readFile(path);
    QVERIFY(!before.isEmpty());
    QTemporaryDir temp;
    QgsProject project;
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {path}, temp.path());
    QVERIFY(!result.ok());
    QVERIFY2(result.retryableDownload, qPrintable(result.error));
    QVERIFY(project.mapLayers().isEmpty());
    QCOMPARE(readFile(path), before);
  }
  void onlyIncompleteDownloadsRequestRetry_data() {
    QTest::addColumn<QString>("fixture");
    QTest::addColumn<bool>("retryable");
    QTest::newRow("missing-central-directory") << QStringLiteral("truncated") << true;
    QTest::newRow("zip-without-shp") << QStringLiteral("no-shp") << true;
    QTest::newRow("zip-missing-shp-companions") << QStringLiteral("missing-pairs") << true;
    QTest::newRow("raw-shp-missing-companions") << QStringLiteral("raw") << false;
    QTest::newRow("library-is-a-file") << QStringLiteral("storage") << false;
    QTest::newRow("provider-cannot-open-layer") << QStringLiteral("invalid-layer") << false;
  }
  void onlyIncompleteDownloadsRequestRetry() {
    QFETCH(QString, fixture);
    QFETCH(bool, retryable);
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto source = temp.filePath(fixture == QLatin1String("raw")
        ? QStringLiteral("source.shp") : QStringLiteral("source.zip"));
    if (fixture == QLatin1String("raw")) {
      QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly));
      QCOMPARE(file.write("incomplete shape"), qint64(16)); file.close();
    } else {
      QList<QPair<QString, QByteArray>> entries;
      entries.append({fixture == QLatin1String("no-shp") ? QStringLiteral("notice.txt")
                                                         : QStringLiteral("site.shp"), QByteArray("fixture")});
      if (fixture == QLatin1String("invalid-layer")) {
        for (const auto& suffix : {"shx", "dbf", "prj"})
          entries.append({QStringLiteral("site.%1").arg(QString::fromLatin1(suffix)), QByteArray("invalid")});
      }
      QVERIFY(writeZip(source, entries));
      if (fixture == QLatin1String("truncated")) {
        auto bytes = readFile(source);
        const auto central = bytes.indexOf(QByteArray::fromHex("504b0102")); QVERIFY(central >= 0);
        bytes.truncate(central);
        QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size()); file.close();
      }
    }
    const auto original = readFile(source);
    const auto library = temp.filePath(QStringLiteral("library"));
    if (fixture == QLatin1String("storage")) {
      QFile blocker(library); QVERIFY(blocker.open(QIODevice::WriteOnly));
      QCOMPARE(blocker.write("keep"), qint64(4)); blocker.close();
    }
    QgsProject project;
    auto* existing = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5179"),
                                        QStringLiteral("existing"), QStringLiteral("memory"));
    QVERIFY(existing->isValid()); project.addMapLayer(existing);
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {source}, library);
    QVERIFY(!result.ok()); QVERIFY(!result.error.isEmpty()); QVERIFY(result.layers.isEmpty());
    QCOMPARE(result.retryableDownload, retryable);
    QCOMPARE(project.mapLayers().size(), 1); QCOMPARE(project.mapLayer(existing->id()), existing);
    QCOMPARE(readFile(source), original);
    if (fixture == QLatin1String("storage")) QCOMPARE(readFile(library), QByteArray("keep"));
  }
  void zipEncodingRestoresThreadLocalSetting_data() {
    QTest::addColumn<bool>("defined");
    QTest::addColumn<QByteArray>("value");
    QTest::newRow("unset") << false << QByteArray();
    QTest::newRow("empty-value") << true << QByteArray("");
    QTest::newRow("different-encoding") << true << QByteArray("UTF-8");
  }
  void zipEncodingRestoresThreadLocalSetting() {
    QFETCH(bool, defined);
    QFETCH(QByteArray, value);
    const char* old = CPLGetThreadLocalConfigOption("CPL_ZIP_ENCODING", nullptr);
    const auto saved = old ? std::optional<QByteArray>(old) : std::nullopt;
    const auto restore = qScopeGuard([saved]() {
      CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", saved ? saved->constData() : nullptr);
    });
    CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", defined ? value.constData() : nullptr);
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("empty-data.zip"));
    QVERIFY(writeZip(source, {{QStringLiteral("notice.txt"), QByteArray("fixture")}}));
    QgsProject project;
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {source}, temp.filePath(QStringLiteral("library")));
    QVERIFY(result.retryableDownload); QVERIFY(project.mapLayers().isEmpty());
    const char* after = CPLGetThreadLocalConfigOption("CPL_ZIP_ENCODING", nullptr);
    QCOMPARE(after != nullptr, defined);
    if (defined) QCOMPARE(QByteArray(after), value);
  }
  void realZipBecomesStyledReferenceLayers() {
    if (!QFileInfo::exists(samplePath()))
      QSKIP("표본 ZIP 이 없습니다(build/qa/heritage-sample/지정유산.zip).");

    QTemporaryDir work;
    QVERIFY(work.isValid());
    QgsProject project;

    const HeritageImport::Result result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {samplePath()}, work.path());

    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY2(!result.layers.isEmpty(), "레이어가 하나도 만들어지지 않았습니다.");
    QVERIFY(!result.retryableDownload);

    // 한 ZIP 에 여러 SHP 가 들어온다. 이름은 파일 이름을 살려야 레이어창에서 구분된다.
    QStringList names;
    for (QgsVectorLayer* layer : result.layers) names << layer->name();
    QVERIFY2(names.contains(QStringLiteral("국가지정유산")), qPrintable(names.join(QLatin1Char(','))));
    QVERIFY(names.contains(QStringLiteral("시도지정유산")));

    QSet<QString> layerColors;
    for (QgsVectorLayer* layer : result.layers) {
      QVERIFY2(layer->isValid(), qPrintable(layer->name()));
      QVERIFY2(layer->featureCount() > 0, qPrintable(layer->name()));

      // 좌표계는 5179 로 온다(사이트 안내: EPSG 5179).
      QVERIFY2(layer->crs().isValid(), qPrintable(layer->name()));

      // 조사 데이터와 섞이지 않게 참조 레이어로 표시되고 읽기 전용이어야 한다.
      QVERIFY(layer->property("readOnly").toBool());

      // 한 레이어 안에서는 색이 하나다(유적마다 갈리지 않는다).
      // 레이어끼리는 서로 다르다 — 아래에서 따로 확인한다.
      if (auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer())) {
        QString one;
        for (const QgsRendererCategory& c : cat->categories()) {
          QVERIFY(c.symbol());
          if (one.isEmpty()) one = c.symbol()->color().name();
          QCOMPARE(c.symbol()->color().name(), one);
        }
        if (!one.isEmpty()) layerColors.insert(one);
      }

      // 레이어창에는 종류 한 줄만, 도면 범례에는 유적명 전부.
      QVERIFY(HeritageSiteLegend::isInstalled(layer));
      QgsLayerTreeLayer* panelNode = project.layerTreeRoot()->findLayer(layer->id());
      QVERIFY(panelNode);
      QList<QgsLayerTreeModelLegendNode*> panel =
          layer->legend()->createLayerTreeModelLegendNodes(panelNode);
      QCOMPARE(panel.size(), 1);
      qDeleteAll(panel);
    }

    // 지정유산 안의 6종은 **같은 색**이다. 구분은 색이 아니라 그룹으로 한다
    // (2026-09-13 사용자 확인: 6종은 전체 종류가 아니라 지정유산의 갈래다).
    QCOMPARE(layerColors.size(), 1);
    QCOMPARE(*layerColors.constBegin(),
             HeritageStyle::color(HeritageDataset::DesignatedHeritage).name());

    // 레이어창 구조: 참조 지도 › 지정유산 › 국가지정유산 …
    QgsLayerTreeGroup* reference =
        project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
    QVERIFY2(reference, "「참조 지도」 그룹이 없습니다.");
    QgsLayerTreeGroup* kind =
        reference->findGroup(HeritageStyle::layerName(HeritageDataset::DesignatedHeritage));
    QVERIFY2(kind, "「지정유산」 그룹이 없습니다.");
    for (QgsVectorLayer* layer : result.layers)
      QVERIFY2(kind->findLayer(layer->id()), qPrintable(layer->name()));
  }

  void siteNameFieldIsPickedFromRealAttributes() {
    if (!QFileInfo::exists(samplePath()))
      QSKIP("표본 ZIP 이 없습니다.");

    QTemporaryDir work;
    QgsProject project;
    const HeritageImport::Result result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {samplePath()}, work.path());
    QVERIFY(!result.layers.isEmpty());

    // 유적명 컬럼은 실제 필드에서 고른다. 이름을 지어내지 않는다.
    for (QgsVectorLayer* layer : result.layers) {
      const QString field = HeritageImport::chooseNameField(layer);
      if (field.isEmpty()) continue;  // 못 고르면 호출자가 알린다(그 자체는 실패가 아니다)
      QVERIFY2(layer->fields().indexOf(field) >= 0, qPrintable(field));
    }
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageImportTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_import.moc"
