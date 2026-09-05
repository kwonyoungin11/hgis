#include <QtTest>
#include <QImage>
#include <QFileDialog>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QMessageBox>

#include "app/MainWindow.h"
#include "app/KaCaptureMapTool.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeregistrybridge.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsvectorlayer.h>

class TestSaveOpen : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_files;
  QString makeSurvey(const QString& name, bool registryOnly = false) {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), name, &error, QStringLiteral("EPSG:5187"));
    if (path.isEmpty()) return {};
    QgsProject project;
    project.setTitle(name);
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"),
                                             name, &error);
    if (!layer || !layer->startEditing()) return {};
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("survey_name"), name);
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
    if (!layer->addFeature(feature) || !layer->commitChanges()) return {};
    layer->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("217,43,43,255")}}).release()));
    if (registryOnly) {
      // Reproduce field files whose data registry survived but whose legend was lost.
      project.layerTreeRegistryBridge()->setEnabled(false);
      project.layerTreeRoot()->removeLayer(layer);
      project.layerTreeRegistryBridge()->setEnabled(true);
    }
    const QString rasterPath = m_files.filePath(QStringLiteral("background.png"));
    QImage pixel(8, 8, QImage::Format_RGB32);
    pixel.fill(Qt::white);
    if (!pixel.save(rasterPath)) return {};
    QFile worldFile(m_files.filePath(QStringLiteral("background.pgw")));
    if (!worldFile.open(QIODevice::WriteOnly)) return {};
    worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n");
    worldFile.close();
    for (const QString& title : {QStringLiteral("위성"), QStringLiteral("지적")}) {
      auto* raster = new QgsRasterLayer(rasterPath, title, QStringLiteral("gdal"));
      if (!raster->isValid()) { delete raster; return {}; }
      raster->setCrs(project.crs());
      project.addMapLayer(raster);
    }
    if (!SurveyStorage::writeEmbedded(&project, path, &error)) return {};
    return path;
  }
  static void disableRendering(MainWindow& window) {
    window.setRestoreLastSurveyEnabled(false);
    if (auto* canvas = window.findChild<QgsMapCanvas*>()) canvas->setRenderFlag(false);
  }
  static QByteArray contents(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
  }
  static QTimer* autosaveTimer(MainWindow& window) {
    for (auto* timer : window.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly))
      if (timer->interval() == 20000) return timer;
    return nullptr;
  }
private slots:
  void cleanup() { QgsProject::instance()->clear(); }
  void saveAndReopen_keepsTreeGeometryAttributesAndStyle() {
    const QString path = makeSurvey(QStringLiteral("왕복조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    auto* originalModel = tree->layerTreeModel();
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QVERIFY(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()));
    QVERIFY(layer->startEditing());
    const auto id = *layer->allFeatureIds().constBegin();
    QVERIFY(layer->changeAttributeValue(id, layer->fields().indexOf(QStringLiteral("survey_name")),
                                        QStringLiteral("수정한 조사")));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(path));
    layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QCOMPARE(layer->getFeature(id).attribute(QStringLiteral("survey_name")).toString(),
             QStringLiteral("수정한 조사"));
    QVERIFY(layer->getFeature(id).geometry().equals(
        QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)))) ;
    auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer->id());
    QVERIFY2(node, "Saved data must have a legend node after reopening");
    QVERIFY(node->isVisible());
    QCOMPARE(tree->layerTreeModel(), originalModel);
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(renderer);
    QCOMPARE(renderer->symbol()->color(), QColor(217, 43, 43));
    QCOMPARE(QgsProject::instance()->crs().authid(), QStringLiteral("EPSG:5187"));
  }
  void open_repairsRegistryOnlyLayers() {
    const QString path = makeSurvey(QStringLiteral("목록복구"), true);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QVERIFY2(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()),
             "Registry-only survey must be restored to the legend");
    QVERIFY(window.findChild<QgsMapCanvas*>()->layers().contains(layer));
  }
  void open_blocksNestedOpenAndAutosave() {
    const QString first = makeSurvey(QStringLiteral("이전조사"));
    const QString second = makeSurvey(QStringLiteral("다음조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    const QByteArray before = contents(first);
    QVERIFY(!before.isEmpty());
    bool invoked = false;
    bool nestedOpened = true;
    auto* autosave = autosaveTimer(window);
    QVERIFY(autosave);
    bool autosaveInvoked = false;
    const auto connection = connect(QgsProject::instance(), &QgsProject::readProject,
        &window, [&](const QDomDocument&) {
          if (invoked) return;
          invoked = true;
          autosaveInvoked = QMetaObject::invokeMethod(autosave, "timeout", Qt::DirectConnection);
          nestedOpened = window.openSurveyGpkg(first);
        });
    const bool opened = window.openSurveyGpkg(second);
    disconnect(connection);
    QVERIFY(invoked);
    QVERIFY(autosaveInvoked);
    QVERIFY(opened);
    QVERIFY2(!nestedOpened, "A nested open must not replace a project being read");
    QCOMPARE(contents(first), before);
    QgsProject savedFirst;
    QVERIFY(savedFirst.read(SurveyStorage::projectUri(first),
                            Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(savedFirst.title(), QStringLiteral("이전조사"));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer);
    QCOMPARE(layer->getFeature(*layer->allFeatureIds().constBegin())
             .attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("다음조사"));
  }
  void failedCommit_doesNotOverwriteSavedWorkspace() {
    const QString path = makeSurvey(QStringLiteral("커밋실패"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    QVERIFY(layer->startEditing());
    QVERIFY(layer->changeAttributeValue(*layer->allFeatureIds().constBegin(),
        layer->fields().indexOf(QStringLiteral("survey_name")), QStringLiteral("저장실패한 편집")));
    layer->setAllowCommit(false);
    project->setTitle(QStringLiteral("커밋에 실패하면 이 작업공간도 쓰지 않는다"));
    const QByteArray before = contents(path);
    QVERIFY(!before.isEmpty());
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    dismiss.stop();
    const QByteArray after = contents(path);
    const bool stillModified = layer->isModified();
    layer->setAllowCommit(true);
    layer->rollBack();
    QVERIFY2(stillModified, "A failed save must retain the edit buffer for retry");
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                       Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("커밋실패"));
    QCOMPARE(after, before);
  }
  void invalidSurvey_doesNotReplaceCurrentWork_data() {
    QTest::addColumn<QByteArray>("invalidContents");
    QTest::newRow("corrupt-file") << QByteArray("not a GeoPackage");
    QTest::newRow("other-vector-format") << QByteArray(
        R"({"type":"FeatureCollection","features":[{"type":"Feature","properties":{},"geometry":{"type":"Point","coordinates":[127,37]}}]})");
  }
  void emptyGpkgWithoutWorkspace_opensWithoutDomainLayers() {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("빈조사"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    // Exercise the on-disk schema alone, without the factory's companion layer tree.
    const QFileInfo file(path);
    QVERIFY(QFile::remove(file.dir().filePath(file.completeBaseName() + QStringLiteral(".qgz"))));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(window.domainLayerCount(), 0);
  }
  void invalidSurvey_doesNotReplaceCurrentWork() {
    QFETCH(QByteArray, invalidContents);
    const QString path = makeSurvey(QStringLiteral("유지할조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QStringList layerIds = QgsProject::instance()->mapLayers().keys();
    auto* autosave = autosaveTimer(window);
    QVERIFY(autosave);
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 유지한 작업"));
    const QString invalid = m_files.filePath(QStringLiteral("잘못된.gpkg"));
    QFile file(invalid);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(invalidContents), invalidContents.size());
    file.close();
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    const bool opened = window.openSurveyGpkg(invalid);
    dismiss.stop();
    QVERIFY2(!opened, "Invalid survey must not be reported as opened");
    QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
    QCOMPARE(QgsProject::instance()->title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QVERIFY(QMetaObject::invokeMethod(autosave, "timeout", Qt::DirectConnection));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QCOMPARE(contents(invalid), invalidContents);
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 수동 저장"));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 수동 저장"));
    QCOMPARE(contents(invalid), invalidContents);
  }
  void openWhileDrawing_resetsCaptureBeforeReplacingLayers() {
    const QString first = makeSurvey(QStringLiteral("그리던조사"));
    const QString second = makeSurvey(QStringLiteral("전환한조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    auto* project = QgsProject::instance();
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(oldLayer);
    auto* capture = window.findChild<KaCaptureMapTool*>();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(capture && canvas);
    QCOMPARE(canvas->mapTool(), capture);
    const QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(190300, 560300, 190350, 560350));
    capture->geometryCaptured(geometry);
    QCOMPARE(oldLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
    QVERIFY(window.openSurveyGpkg(second));
    QVERIFY(oldLayer.isNull());
    QVERIFY2(canvas->mapTool() != capture, "Opening another survey must stop the previous drawing session");
    QCOMPARE(capture->pointCount(), 0);
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    capture->geometryCaptured(geometry); // A late capture cannot use the deleted edit layer.
    QCOMPARE(layer->featureCount(), 1LL);
    window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->setCurrentLayer(layer);
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    capture->geometryCaptured(geometry);
    auto* newDrawingLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(newDrawingLayer);
    QCOMPARE(newDrawingLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
  }
  void saveAs_preservesActualTablesAndReopensCopy() {
    const QString path = makeSurvey(QStringLiteral("여러구역"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* second = LayerOps::createSurveyAreaLayer(project, path, QStringLiteral("두번째 구역"),
                                                  Qt::black, QColor(25, 80, 170), 0.3, &error);
    QVERIFY2(second, qPrintable(error));
    QVERIFY(second->startEditing());
    QgsFeature feature(second->fields());
    QVERIFY(feature.setAttribute(QStringLiteral("name"), QStringLiteral("독립 구역")));
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560200, 190250, 560250)));
    QVERIFY(second->addFeature(feature));
    QVERIFY(second->commitChanges());
    const QString secondId = second->id();
    const QString tableOptions = second->source().section(QLatin1Char('|'), 1);
    const QString target = m_files.filePath(QStringLiteral("새 이름.gpkg"));
    QTimer chooseFile;
    connect(&chooseFile, &QTimer::timeout, [&] {
      for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* dialog = qobject_cast<QFileDialog*>(widget)) {
          dialog->selectFile(target);
          QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
          chooseFile.stop();
        }
      }
    });
    chooseFile.start(20);
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProjectAs", Qt::DirectConnection));
    chooseFile.stop();
    QVERIFY(QFile::exists(target));
    QCOMPARE(second->source().section(QLatin1Char('|'), 1), tableOptions);
    QCOMPARE(QFileInfo(second->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(target).absoluteFilePath());
    QVERIFY(window.openSurveyGpkg(target));
    second = qobject_cast<QgsVectorLayer*>(project->mapLayer(secondId));
    QVERIFY(second && second->isValid());
    QCOMPARE(second->featureCount(), 1LL);
    QCOMPARE(second->getFeature(*second->allFeatureIds().constBegin())
                 .attribute(QStringLiteral("name")).toString(), QStringLiteral("독립 구역"));
  }
  void fieldCopy_restoresAllValidLayersAndSavesTree() {
    const QString input = qEnvironmentVariable("KA_HGIS_FIELD_GPKG");
    if (input.isEmpty()) QSKIP("Set KA_HGIS_FIELD_GPKG to a disposable field-file copy");
    const QString copy = m_files.filePath(QFileInfo(input).fileName());
    QVERIFY(QFile::copy(input, copy));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(copy));
    auto* project = QgsProject::instance();
    QMap<QString, qint64> counts;
    for (auto* layer : project->mapLayers()) {
      if (!layer->isValid()) continue;
      QVERIFY2(project->layerTreeRoot()->findLayer(layer->id()), qPrintable(layer->name()));
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
        QCOMPARE(QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).canonicalFilePath(),
                 QFileInfo(copy).canonicalFilePath());
        counts.insert(vector->id(), vector->featureCount());
      }
    }
    QVERIFY(!counts.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(copy));
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
      auto* vector = qobject_cast<QgsVectorLayer*>(project->mapLayer(it.key()));
      QVERIFY(vector && vector->isValid());
      QCOMPARE(vector->featureCount(), it.value());
      QVERIFY(project->layerTreeRoot()->findLayer(vector->id()));
    }
    qInfo() << "Field copy restored vector layers:" << counts.size()
            << "registry:" << project->mapLayers().size()
            << "legend:" << project->layerTreeRoot()->findLayers().size();
    const QString screenshot = qEnvironmentVariable("KA_HGIS_FIELD_SCREENSHOT");
    if (!screenshot.isEmpty()) {
      window.show();
      window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->expandAll();
      QApplication::processEvents();
      QVERIFY(window.grab().save(screenshot));
    }
  }
};

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QgsApplication app(argc, argv, true);
  QTemporaryDir settings;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  TestSaveOpen tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_save_open.moc"
