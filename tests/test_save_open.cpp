#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QImage>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabWidget>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <gdal.h>
#include <cpl_error.h>
#include <cpl_conv.h>
#include <memory>

#include "app/MainWindow.h"
#include "app/KaCaptureMapTool.h"
#include "app/KaAttributeMapTool.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaVertexEditTool.h"
#include "app/KaDrawingStudio.h"
#include "app/KaTheme.h"
#include "app/KaRegionLocator.h"
#include "core/LayerOps.h"
#include "core/RecentSurveys.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsexception.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeregistrybridge.h>
#include <qgslayertreeview.h>
#include <qgslayout.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutview.h>
#include <qgsmapcanvas.h>
#include <qgsmaptopixel.h>
#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayerlabeling.h>
#include <qgspallabeling.h>

static QString s_testSettingsPath;

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
  // 20초 자동 저장은 없앴다. 저장은 「저장」을 누를 때만 일어난다. 예전에는 이 타이머를
  // 찾아 timeout 을 쏘아 자동 저장을 흉내 냈지만, 이제 저장 경로를 직접 부른다.
  static bool saveNow(MainWindow& window) {
    bool ok = false;
    return QMetaObject::invokeMethod(&window, "persistSurveyWork", Qt::DirectConnection,
                                     Q_RETURN_ARG(bool, ok)) && ok;
  }
  static bool hasNoAutosaveTimer(MainWindow& window) {
    for (auto* timer : window.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly))
      if (timer->isActive() && timer->objectName() != QLatin1String("layerWatchTimer") &&
          timer->interval() > 0 && timer->interval() <= 60000)
        return false;
    return true;
  }
  struct MenuActionState {
    QString id;
    QString text;
    QString toolTip;
    bool enabled = false;
    bool separator = false;
  };
  struct LayerMenuState {
    bool seen = false;
    QString name;
    QList<MenuActionState> actions;
  };
  static LayerMenuState inspectLayerMenu(MainWindow& window, QgsLayerTreeView* tree,
                                         const QPoint& viewportPosition,
                                         const QString& triggerId = {}) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&window, &state, triggerId] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      const QString sample = window.property("qaMenuKind").toString();
      if (!output.isEmpty() && !sample.isEmpty())
        menu->grab().save(QDir(output).filePath(QStringLiteral("menu-%1.png").arg(sample)));
      if (!triggerId.isEmpty()) {
        for (auto* action : menu->actions()) {
          if (action->objectName() == triggerId && action->isEnabled()) {
            action->trigger();
            break;
          }
        }
      }
      menu->close();
    });
    inspect.start(0);
    window.showLayerTreeContextMenu(tree, viewportPosition);
    return state;
  }
  static LayerMenuState inspectMapMenu(MainWindow& window, const QPoint& point) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&state] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      menu->close();
    });
    inspect.start(0);
    if (!QMetaObject::invokeMethod(&window, "onMapContextMenu", Qt::DirectConnection, Q_ARG(QPoint, point)))
      return {};
    return state;
  }
  QgsMapLayer* makeMenuReference(const QString& kind) {
    std::unique_ptr<QgsMapLayer> layer;
    if (kind == QLatin1String("satellite")) {
      layer = std::make_unique<QgsRasterLayer>(
          QStringLiteral("type=xyz&url=https://menu-fixture.invalid/{z}/{x}/{y}.png&zmin=0&zmax=18"),
          QStringLiteral("위성"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("cadastral")) {
      QUrlQuery uri;
      uri.addQueryItem(QStringLiteral("url"), QUrl::fromLocalFile(
          QDir(s_testSettingsPath).filePath(QStringLiteral("cadastral-capabilities.xml"))).toString());
      uri.addQueryItem(QStringLiteral("layers"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("styles"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("format"), QStringLiteral("image/png"));
      uri.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:5187"));
      layer = std::make_unique<QgsRasterLayer>(uri.toString(QUrl::FullyEncoded),
                                               QStringLiteral("지적"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("dem")) {
      const QString path = m_files.filePath(QStringLiteral("menu-elevation.tif"));
      GDALDriverH driver = GDALGetDriverByName("GTiff");
      if (!driver) return nullptr;
      GDALDatasetH dataset = GDALCreate(driver, path.toUtf8().constData(), 4, 4, 1, GDT_Float32, nullptr);
      if (!dataset) return nullptr;
      double transform[] = {190000.0, 10.0, 0.0, 560040.0, 0.0, -10.0};
      const bool written = GDALSetGeoTransform(dataset, transform) == CE_None &&
          GDALSetProjection(dataset, QgsProject::instance()->crs().toWkt().toUtf8().constData()) == CE_None &&
          GDALFillRaster(GDALGetRasterBand(dataset, 1), 100.0, 0.0) == CE_None;
      GDALClose(dataset);
      if (!written) return nullptr;
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
    } else if (kind == QLatin1String("imported_raster")) {
      const QString path = m_files.filePath(QStringLiteral("menu-imported.png"));
      QImage image(8, 8, QImage::Format_RGB32);
      image.fill(Qt::white);
      if (!image.save(path)) return nullptr;
      QFile worldFile(m_files.filePath(QStringLiteral("menu-imported.pgw")));
      if (!worldFile.open(QIODevice::WriteOnly) ||
          worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n") < 0) return nullptr;
      worldFile.close();
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("가져온 현장 도면"), QStringLiteral("gdal"));
      layer->setCrs(QgsProject::instance()->crs());
      layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    } else if (kind == QLatin1String("external_shp")) {
      QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"),
                            QStringLiteral("fixture"), QStringLiteral("memory"));
      QgsFeature feature(source.fields());
      feature.setAttribute(0, QStringLiteral("외부 경계"));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      if (!source.isValid() || !source.dataProvider()->addFeature(feature)) return nullptr;
      const QString path = m_files.filePath(QStringLiteral("menu-external.shp"));
      QgsVectorFileWriter::SaveVectorOptions options;
      options.driverName = QStringLiteral("ESRI Shapefile");
      options.fileEncoding = QStringLiteral("UTF-8");
      if (QgsVectorFileWriter::writeAsVectorFormatV3(&source, path,
          QgsProject::instance()->transformContext(), options) != QgsVectorFileWriter::NoError)
        return nullptr;
      // A cadastral-looking display name must not turn an actual vector into WMS.
      layer = std::make_unique<QgsVectorLayer>(path, QStringLiteral("지적 외부 경계"), QStringLiteral("ogr"));
    }
    if (!layer || !layer->isValid()) return nullptr;
    if (kind != QLatin1String("external_shp")) LayerOps::markReferenceLayer(layer.get());
    return QgsProject::instance()->addMapLayer(layer.release());
  }
private slots:
  void cleanup() { QgsProject::instance()->clear(); }
  void provinceChipMovesMapWithoutLoadingLayers_data() {
    QTest::addColumn<QString>("crs");
    for (const QString& crs : {QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")})
      QTest::newRow(qPrintable(crs)) << crs;
  }
  void provinceChipMovesMapWithoutLoadingLayers() {
    QFETCH(QString, crs);
    MainWindow window;
    disableRendering(window);
    window.resize(1800, 1000);
    window.show();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* locator = window.findChild<KaRegionLocator*>();
    QVERIFY(canvas && locator);
    QVERIFY(QMetaObject::invokeMethod(&window, crs.endsWith('6') ? "setWorkCrs5186" : "setWorkCrs5187", Qt::DirectConnection));
    const auto layerIds = QgsProject::instance()->mapLayers().keys();
    const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                           QgsCoordinateReferenceSystem(crs), QgsProject::instance());
    struct Visit { const char* chip; double lon; double lat; };
    for (const Visit& visit : {Visit{"서울", 126.978, 37.5665}, Visit{"울산", 129.3114, 35.5396},
                              Visit{"제주", 126.531, 33.4996}}) {
      QToolButton* chip = nullptr;
      for (auto* button : locator->findChildren<QToolButton*>())
        if (button->text() == QString::fromUtf8(visit.chip)) chip = button;
      QVERIFY(chip);
      chip->click();
      QCoreApplication::processEvents();
      QCOMPARE(canvas->mapSettings().destinationCrs().authid(), crs);
      QCOMPARE(QgsProject::instance()->crs().authid(), crs);
      const QgsPointXY expected = transform.transform(QgsPointXY(visit.lon, visit.lat));
      QVERIFY2(canvas->extent().contains(expected), qPrintable(QStringLiteral("%1 is outside %2").arg(chip->text(), canvas->extent().toString())));
      QVERIFY(canvas->extent().width() > 10000.);
      QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
      QVERIFY(!locator->findChild<QPushButton*>(QStringLiteral("regionFieldMap")));
    }
    QgsProject::instance()->setDirty(false);
  }
  void drawingStudio_largeScaleChipsApplyToMapAndInput() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 297.0, 210.0);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    view->setUpdatesEnabled(false);
    studio.show();
    // Complete the studio's queued initial map placement before selecting a scale.
    QCoreApplication::processEvents();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    QCOMPARE(map->crs().authid(), QStringLiteral("EPSG:5187"));
    auto* spin = studio.findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    QVERIFY(spin);
    const QFontMetrics font(spin->font());
    QVERIFY(font.inFont(QChar(u'1')));
    QVERIFY(font.inFont(QChar(u'한')));
    auto* input = spin->findChild<QLineEdit*>();
    QVERIFY(input);
    const auto chips = studio.findChildren<QToolButton*>(QStringLiteral("scaleChip"));
    // Exercise both new choices and a return to an existing choice through the
    // actual clicked signal; setting the spinbox directly would miss bad wiring.
    for (const int denominator : {10000, 25000, 5000}) {
      QToolButton* target = nullptr;
      int matches = 0;
      for (QToolButton* chip : chips) {
        if (chip->property("denom").toInt() == denominator) {
          target = chip;
          ++matches;
        }
      }
      QCOMPARE(matches, 1);
      QVERIFY(target && target->isEnabled());
      target->click();
      QCOMPARE(spin->value(), denominator);
      QCOMPARE(input->text(), QString::number(denominator));
      for (QToolButton* chip : chips)
        QCOMPARE(chip->isChecked(), chip->property("denom").toInt() == denominator);
      QVERIFY2(qAbs(map->scale() - denominator) < 0.5,
               qPrintable(QStringLiteral("chip 1:%1 applied map scale %2")
                              .arg(denominator).arg(map->scale(), 0, 'f', 3)));
    }
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty() && QDir(output).exists()) {
      const QString path = QDir(output).filePath(QStringLiteral("drawing-scale-chips-widget-render.png"));
      QVERIFY2(spin->parentWidget()->grab().save(path), qPrintable(path));
      qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
    }
  }
  void layerContextMenu_matchesLayerKind_data() {
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("firstAction");
    QTest::newRow("survey-area") << QStringLiteral("survey_area") << QStringLiteral("layer.import");
    QTest::newRow("feature-polygon") << QStringLiteral("feature_poly") << QStringLiteral("layer.import");
    QTest::newRow("control-points") << QStringLiteral("control_points") << QStringLiteral("layer.import");
    QTest::newRow("section-line") << QStringLiteral("section_line") << QStringLiteral("layer.import");
    QTest::newRow("trial-trench") << QStringLiteral("trial_trench") << QStringLiteral("layer.import");
    QTest::newRow("satellite-xyz") << QStringLiteral("satellite") << QStringLiteral("layer.import");
    QTest::newRow("cadastral-wms") << QStringLiteral("cadastral") << QStringLiteral("layer.import");
    QTest::newRow("dem-raster") << QStringLiteral("dem") << QStringLiteral("layer.import");
    QTest::newRow("external-shapefile") << QStringLiteral("external_shp") << QStringLiteral("layer.import");
    QTest::newRow("imported-reference-raster") << QStringLiteral("imported_raster") << QStringLiteral("layer.import");
  }
  void layerContextMenu_matchesLayerKind() {
    QFETCH(QString, kind);
    QFETCH(QString, firstAction);
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("menu_%1").arg(kind), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    MainWindow window;
    window.setProperty("qaMenuKind", kind);
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    const bool domain = LayerOps::domainLayerKeys().contains(kind);
    QgsMapLayer* layer = domain ? LayerOps::ensureDomainLayer(
        QgsProject::instance(), path, kind, kind, &error) : makeMenuReference(kind);
    QVERIFY2(layer && layer->isValid(), qPrintable(error));
    if (domain) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      QVERIFY(vector && vector->startEditing());
      QgsFeature feature(vector->fields());
      if (vector->geometryType() == Qgis::GeometryType::Point)
        feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(190000, 560000)));
      else if (vector->geometryType() == Qgis::GeometryType::Line)
        feature.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(190000, 560000), QgsPointXY(190100, 560100)}));
      else
        feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      QVERIFY(vector->addFeature(feature));
      QVERIFY(vector->commitChanges());
    }
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    QApplication::processEvents();
    const QModelIndex index = tree->layerTreeModel()->node2index(
        QgsProject::instance()->layerTreeRoot()->findLayer(layer));
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
    QVERIFY(menu.seen);
    QCOMPARE(menu.name, QStringLiteral("layerContextMenu"));
    QStringList ids;
    for (const auto& action : menu.actions) {
      if (action.separator) continue;
      QVERIFY2(!action.id.isEmpty(), qPrintable(action.text));
      ids.append(action.id);
      if (!action.enabled) {
        QVERIFY2(!action.toolTip.trimmed().isEmpty(), qPrintable(action.id));
        QVERIFY2(action.toolTip != action.text, qPrintable(action.id));
      }
    }
    // Common positions are intentional: the user requested the earlier shared menu.
    QCOMPARE(ids.first(), firstAction);
    QCOMPARE(ids.last(), QStringLiteral("layer.remove"));
    int destructiveStart = menu.actions.size() - 1;
    if (ids.contains(QStringLiteral("layer.clear"))) {
      QVERIFY(domain);
      QCOMPARE(menu.actions.at(destructiveStart - 1).id, QStringLiteral("layer.clear"));
      --destructiveStart;
    }
    QVERIFY(destructiveStart > 0);
    QVERIFY(menu.actions.at(destructiveStart - 1).separator);
    for (const QString& common : {QStringLiteral("layer.import"), QStringLiteral("layer.rename"),
        QStringLiteral("layer.style"), QStringLiteral("layer.attributes"), QStringLiteral("layer.labels"),
        QStringLiteral("layer.opacity"), QStringLiteral("layer.zoom"), QStringLiteral("layer.fullExtent")})
      QVERIFY2(ids.contains(common), qPrintable(common));
    if (!domain) QVERIFY(!ids.contains(QStringLiteral("layer.clear")));
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
      // Removal keeps the live layer and edit buffer in undo history.
      QVERIFY(vector->startEditing());
      QgsFeature extra = vector->getFeature(*vector->allFeatureIds().constBegin());
      extra.setId(FID_NULL);
      QVERIFY(vector->addFeature(extra));
      QVERIFY(vector->isModified());
      const LayerMenuState dirtyMenu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
      QVERIFY(dirtyMenu.seen);
      bool removeFound = false;
      for (const auto& action : dirtyMenu.actions) {
        if (action.id != QLatin1String("layer.remove")) continue;
        removeFound = true;
        QVERIFY(action.enabled);
        QVERIFY(!action.toolTip.trimmed().isEmpty());
        QVERIFY(action.toolTip != action.text);
      }
      QVERIFY(removeFound);
      QVERIFY(vector->rollBack());
    }
  }
  void changingLabelFontKeepsEveryLayerAndItsVisibility() {
    const QString path = makeSurvey(QStringLiteral("글자 크기 검증"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && tree && canvas);
    QVERIFY(LayerOps::applyAreaM2Labels(layer));
    const QString areaExpression = layer->labeling()->settings().fieldName;
    const auto ids = project->mapLayers().keys();
    QMap<QString, bool> visibility;
    for (auto* node : project->layerTreeRoot()->findLayers()) visibility[node->layerId()] = node->itemVisibilityChecked();
    window.resize(1800, 1000);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    canvas->setExtent(QgsRectangle(189980, 559980, 190130, 560130));
    QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
    canvas->setRenderFlag(true);
    canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-before-font.png"))));
    bool changed = false;
    bool areaChecked = false;
    QTimer::singleShot(0, &window, [&]() {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      auto* areaAction = menu->findChild<QAction*>(QStringLiteral("layer.labelArea"));
      areaChecked = areaAction && areaAction->isChecked();
      auto* sizeAction = menu->findChild<QAction*>(QStringLiteral("layer.labelSize"));
      auto* sizes = sizeAction ? sizeAction->menu() : nullptr;
      if (sizes) {
        for (auto* action : sizes->actions()) if (action->text() == QLatin1String("12 pt")) {
          action->trigger(); changed = true; break;
        }
      }
      menu->close();
    });
    const QModelIndex index = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(layer));
    window.showLayerTreeContextMenu(tree, tree->visualRect(index).center());
    QVERIFY(changed);
    QVERIFY(areaChecked);
    QCOMPARE(LayerOps::labelFontSize(layer), 12.0);
    rendered.clear(); canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    QCOMPARE(project->mapLayers().keys(), ids);
    for (auto it = visibility.cbegin(); it != visibility.cend(); ++it) {
      auto* node = project->layerTreeRoot()->findLayer(it.key());
      QVERIFY(node);
      QCOMPARE(node->itemVisibilityChecked(), it.value());
    }
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-after-font.png"))));
    QCOMPARE(layer->labeling()->settings().fieldName, areaExpression);
    QVERIFY(LayerOps::labelShowArea(layer));
    canvas->setRenderFlag(false);
  }

  void layerDeleteKeyPreservesSourceAndUndoRestoresPendingEdits() {
    const QString path = makeSurvey(QStringLiteral("delete_key_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(tree && layer);
    const QString id = layer->id();
    QVERIFY(layer->startEditing());
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const int field = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(fid, field, QStringLiteral("저장 전 기록")));
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(layer);
    tree->setFocus();
    QApplication::processEvents();
    const QByteArray original = contents(path);
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY(!project->mapLayer(id));
    QCOMPARE(contents(path), original);
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project->mapLayer(id), layer);
    QVERIFY(project->layerTreeRoot()->findLayer(id));
    QVERIFY(layer->isModified());
    QCOMPARE(layer->getFeature(fid).attribute(field).toString(), QStringLiteral("저장 전 기록"));
    QCOMPARE(contents(path), original);
    QVERIFY(layer->rollBack());
  }

  void ctrlZRestoresVertexEditsAndGroupedFeatureDeletion() {
    const QString path = makeSurvey(QStringLiteral("vertex_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && canvas);
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const QgsGeometry original = layer->getFeature(fid).geometry();
    QVERIFY(QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    auto* select = window.findChild<KaFeatureSelectTool*>();
    auto* vertex = select ? select->findChild<KaVertexEditTool*>() : nullptr;
    QVERIFY(vertex);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190020, 560020)));
    QVERIFY(!layer->getFeature(fid).geometry().equals(original));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->insertVertexAt(1, QgsPointXY(190040, 560000)));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->deleteVertexAt(1));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    QgsFeature second(layer->fields());
    second.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560000, 190250, 560050)));
    QVERIFY(layer->addFeature(second));
    QVERIFY(layer->commitChanges());
    layer->selectAll();
    canvas->setLayers({layer});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QCOMPARE(layer->featureCount(), 0);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2); // no duplicated fallback history
    // Restoring a deletion into a dirty edit buffer creates a temporary FID.
    // Saving must remap the earlier geometry command to the committed FID.
    const auto restoredIds = layer->allFeatureIds();
    const auto editedId = *restoredIds.constBegin();
    const auto previous = layer->getFeature(editedId).geometry();
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    const int nameField = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(editedId, nameField, QStringLiteral("미저장 기록")));
    vertex->setTarget(layer, editedId);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190025, 560025)));
    layer->selectByIds({editedId});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(saveNow(window));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    bool previousFound = false;
    QgsFeature restored;
    auto iterator = layer->getFeatures();
    while (iterator.nextFeature(restored)) previousFound |= restored.geometry().equals(previous);
    QVERIFY(previousFound);
  }

  void layerContextMenu_usesClickedRowAndLeavesSourceIntact() {
    const QString path = makeSurvey(QStringLiteral("menu_target"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* survey = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(survey);
    auto* external = makeMenuReference(QStringLiteral("external_shp"));
    QVERIFY(external && external->isValid());
    const QString externalId = external->id();
    const QString surveyId = survey->id();
    const QString externalPath = external->source().section(QLatin1Char('|'), 0, 0);
    const QByteArray originalSurvey = contents(path);
    const QByteArray originalShp = contents(externalPath);
    QVERIFY(!originalSurvey.isEmpty() && !originalShp.isEmpty());
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.resize(1280, 900);
    tree->setMinimumHeight(360);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(survey);
    QApplication::processEvents();
    tree->collapseAll();
    const QPoint blank(tree->viewport()->width() / 2, tree->viewport()->height() - 2);
    QVERIFY(!tree->indexAt(blank).isValid());
    const LayerMenuState blankMenu = inspectLayerMenu(window, tree, blank);
    QVERIFY(blankMenu.seen);
    for (const auto& action : blankMenu.actions) {
      QVERIFY(action.id != QLatin1String("layer.remove"));
      QVERIFY(action.id != QLatin1String("layer.clear"));
    }
    tree->expandAll();
    tree->setCurrentLayer(survey);
    const QModelIndex clicked = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(external));
    QVERIFY(clicked.isValid());
    tree->scrollTo(clicked);
    const LayerMenuState clickedMenu = inspectLayerMenu(window, tree,
        tree->visualRect(clicked).center(), QStringLiteral("layer.remove"));
    QVERIFY(clickedMenu.seen);
    QVERIFY(!project->mapLayer(externalId));
    QCOMPARE(project->mapLayer(surveyId), survey);
    QCOMPARE(survey->featureCount(), 1LL);
    QCOMPARE(contents(path), originalSurvey);
    QCOMPARE(contents(externalPath), originalShp);
    // Removing a survey legend entry must likewise preserve its saved features.
    tree->setCurrentLayer(survey);
    const QModelIndex surveyIndex = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(survey));
    tree->scrollTo(surveyIndex);
    const LayerMenuState surveyMenu = inspectLayerMenu(window, tree,
        tree->visualRect(surveyIndex).center(), QStringLiteral("layer.remove"));
    QVERIFY(surveyMenu.seen);
    QVERIFY(!project->mapLayer(surveyId));
    QCOMPARE(contents(path), originalSurvey);
    QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"),
                          QStringLiteral("stored"), QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.featureCount(), 1LL);
  }
  void layerContextMenu_moveToBottomPreservesRegisteredLayer() {
    const QString path = makeSurvey(QStringLiteral("menu_reorder"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* dem = makeMenuReference(QStringLiteral("dem"));
    QVERIFY(dem && dem->isValid());
    const QString source = dem->source();
    const QByteArray originalFile = contents(source);
    QVERIFY(!originalFile.isEmpty());
    LayerOps::placeInLegendGroup(project, dem, QStringLiteral("참조 지도"));
    auto* originalNode = project->layerTreeRoot()->findLayer(dem);
    QVERIFY(originalNode);
    auto* group = qobject_cast<QgsLayerTreeGroup*>(originalNode->parent());
    QVERIFY(group);
    auto* other = new QgsRasterLayer(source, QStringLiteral("다른 고도 자료"), QStringLiteral("gdal"));
    QVERIFY(other->isValid());
    LayerOps::markReferenceLayer(other);
    QVERIFY(project->addMapLayer(other, false));
    group->addLayer(other);
    QVERIFY(group->children().last() != originalNode);
    const QString id = dem->id();
    const QPointer<QgsMapLayer> guard(dem);
    const int layerCount = project->mapLayers().size();
    const int nodeCount = project->layerTreeRoot()->findLayers().size();
    const int siblingCount = group->children().size();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(dem);
    QApplication::processEvents();
    const QModelIndex index = tree->layerTreeModel()->node2index(originalNode);
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center(),
                                                QStringLiteral("layer.bottom"));
    QVERIFY(menu.seen);
    bool enabledBottom = false;
    for (const auto& action : menu.actions)
      if (action.id == QLatin1String("layer.bottom")) enabledBottom = action.enabled;
    QVERIFY(enabledBottom);
    // The registry bridge defers layer deletion after its last tree node disappears.
    // Drain that work before asserting preservation, rather than checking only the
    // synchronous clone/move result.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(guard);
    QCOMPARE(project->mapLayer(id), guard.data());
    QCOMPARE(project->mapLayers().size(), layerCount);
    QCOMPARE(project->layerTreeRoot()->findLayers().size(), nodeCount);
    auto* movedNode = project->layerTreeRoot()->findLayer(id);
    QVERIFY(movedNode);
    QCOMPARE(movedNode->parent(), group);
    QCOMPARE(group->children().size(), siblingCount);
    QCOMPARE(group->children().last(), movedNode);
    QCOMPARE(guard->source(), source);
    QCOMPARE(contents(source), originalFile);
  }
  void mapContextMenu_onlyOffersRecordEditingForSurveyFeatures() {
    const QString path = makeSurvey(QStringLiteral("map_menu_scope"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* survey = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* external = qobject_cast<QgsVectorLayer*>(makeMenuReference(QStringLiteral("external_shp")));
    QVERIFY(survey && external && external->isValid());
    QVERIFY(LayerOps::layerKeyOf(external).isEmpty());
    QCOMPARE(external->providerType(), QStringLiteral("ogr"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    window.show();
    QApplication::processEvents();
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    for (auto* target : {external, survey}) {
      canvas->setLayers({target});
      const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(190050, 560050);
      const QPoint point(qRound(pixel.x()), qRound(pixel.y()));
      KaAttributeMapTool picker(canvas);
      QgsVectorLayer* picked = nullptr;
      QgsFeature feature;
      QVERIFY(picker.pickAtScreen(point, &picked, &feature));
      QCOMPARE(picked, target);
      QVERIFY(feature.isValid());
      const LayerMenuState menu = inspectMapMenu(window, point);
      QVERIFY(menu.seen);
      QCOMPARE(menu.name, QStringLiteral("mapContextMenu"));
      bool recordAction = false;
      int actionCount = 0;
      for (const auto& action : menu.actions) {
        if (action.separator) continue;
        ++actionCount;
        if (action.id == QLatin1String("map.attributes")) {
          recordAction = true;
          QVERIFY(action.enabled);
        }
        if (!action.enabled) {
          QVERIFY(!action.toolTip.trimmed().isEmpty());
          QVERIFY(action.toolTip != action.text);
        }
      }
      QCOMPARE(recordAction, target == survey);
      QVERIFY(actionCount <= 10);
    }
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen_data() {
    QTest::addColumn<QString>("authId");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }
  static bool executeGpkgSql(const QString& path, const char* sql) {
    GDALDatasetH dataset = GDALOpenEx(path.toUtf8().constData(),
        GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr);
    if (!dataset) return false;
    CPLErrorReset();
    OGRLayerH result = GDALDatasetExecuteSQL(dataset, sql, nullptr, nullptr);
    const bool ok = CPLGetLastErrorType() < CE_Failure;
    if (result) GDALDatasetReleaseResultSet(dataset, result);
    GDALClose(dataset);
    return ok;
  }
  static bool selectSaveAs(MainWindow& window, const QString& target) {
    QTimer choose;
    bool selected = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
        if (selected) return;
        dialog->setOption(QFileDialog::DontConfirmOverwrite);
        dialog->setDirectory(QFileInfo(target).absolutePath());
        dialog->selectFile(QFileInfo(target).fileName());
        if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
          name->setText(QFileInfo(target).fileName());
        const QStringList files = dialog->selectedFiles();
        selected = files.size() == 1 && QFileInfo(files.first()).absoluteFilePath() == QFileInfo(target).absoluteFilePath();
        qInfo() << "Save As selected file:" << files << "matches target:" << selected;
        if (!selected) { choose.stop(); dialog->reject(); return; }
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
      } else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        message->accept();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    return QMetaObject::invokeMethod(&window, "saveProjectAs", Qt::DirectConnection) && selected;
  }
  static bool openWithAnswer(MainWindow& window, const QString& path,
                             QMessageBox::StandardButton answer, bool* prompted = nullptr) {
    QTimer choose;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(answer)) {
          if (prompted) *prompted = true;
          question->button(answer)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    return window.openSurveyGpkg(path);
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen() {
    QFETCH(QString, authId);
    const QString name = QStringLiteral("crs_%1").arg(authId.mid(5));
    const QString path = m_files.filePath(name + QStringLiteral(".gpkg"));
    MainWindow window;
    disableRendering(window);
    window.show();
    QTimer choose;
    bool selected = false;
    bool folderChosen = false;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        folder->setDirectory(m_files.path());
        folder->selectFile(m_files.path());
        folderChosen = true;
        choose.stop();
        QMetaObject::invokeMethod(folder, "accept", Qt::DirectConnection);
      } else if (!selected && dialog->windowTitle() == QStringLiteral("새 조사")) {
        dialog->findChild<QLineEdit*>()->setText(name);
        for (auto* button : dialog->findChildren<QPushButton*>()) {
          if (button->text().startsWith(authId.mid(5))) {
            button->click();
            selected = button->isChecked();
          }
        }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(15000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    QVERIFY(selected && folderChosen);
    QVERIFY(QFile::exists(path));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* chip = window.findChild<QToolButton*>(QStringLiteral("crsButton"));
    auto* upload = window.findChild<QLabel*>(QStringLiteral("uploadCrsChip"));
    QVERIFY(canvas && chip && upload);
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"), name, QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.crs().authid(), authId);
    QVERIFY(saveNow(window));
    const QString other = authId.endsWith(QLatin1String("5186"))
        ? QStringLiteral("EPSG:5187") : QStringLiteral("EPSG:5186");
    QTimer selectCrs;
    connect(&selectCrs, &QTimer::timeout, [&] {
      if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        for (auto* action : menu->actions()) {
          if (action->text().contains(other)) {
            menu->setActiveAction(action);
            selectCrs.stop();
            QTest::keyClick(menu, Qt::Key_Return);
            break;
          }
        }
      }
    });
    selectCrs.start(20);
    chip->click();
    selectCrs.stop();
    QCOMPARE(QgsProject::instance()->crs().authid(), other);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), other);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(other.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    QVERIFY(openWithAnswer(window, path, QMessageBox::Discard));
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
  }
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
  void startupHome_ignoresRememberedSurveyUntilUserOpensIt() {
    const QString path = makeSurvey(QStringLiteral("수동열기"));
    QVERIFY(!path.isEmpty());
    QSettings st = RecentSurveys::userSettings();
    QVERIFY2(QFileInfo(st.fileName()).absoluteFilePath().startsWith(
                 QFileInfo(s_testSettingsPath).absoluteFilePath()),
             qPrintable(QStringLiteral("RecentSurveys settings escaped test dir: %1")
                            .arg(st.fileName())));
    RecentSurveys::remember(st, path, QStringLiteral("수동열기"));

    MainWindow window;
    window.show();
    QTest::qWait(250);

    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("홈"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    QCOMPARE(canvas->layers().size(), 0);
    const QString screenshot = qEnvironmentVariable("KA_HGIS_STARTUP_SCREENSHOT");
    if (!screenshot.isEmpty())
      QVERIFY(window.grab().save(screenshot));

    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("지도"));
    QVERIFY(LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area")));
  }
  // 축척 칸은 하나뿐이어야 한다. 예전에는 자유 입력 QLineEdit 과 프리셋 QComboBox 가
  // 따로 있어서 같은 축척인데도 어느 쪽으로 넣었느냐에 따라 화면이 달랐다.
  void scaleControl_isSingleWidgetAcceptingBothForms() {
    MainWindow window;
    disableRendering(window);
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY2(combo, "축척 콤보가 없다");
    QVERIFY2(combo->isEditable(), "축척 콤보는 직접 입력이 돼야 한다");
    QVERIFY2(combo->lineEdit(), "콤보에 입력줄이 있어야 한다");

    // 입력줄은 콤보의 것 하나뿐 — 따로 떠 있는 축척 칸이 있으면 안 된다.
    int standalone = 0;
    for (auto* e : window.findChildren<QLineEdit*>(QStringLiteral("scaleEdit")))
      if (e != combo->lineEdit()) ++standalone;
    QCOMPARE(standalone, 0);

    // 발굴 도면 축척이 프리셋에 있어야 한다.
    QVERIFY2(combo->findData(200) >= 0, "1:200 프리셋이 없다");
    QVERIFY2(combo->findData(500) >= 0, "1:500 프리셋이 없다");

    // "2000" 과 "1:2000" 이 같은 값으로 읽혀야 한 칸으로 합친 의미가 있다.
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral(" 1 : 2,000 ")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:200")), 200.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("메롱")), 0.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QString()), 0.0);
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
    // 주기적으로 파일에 쓰는 타이머가 남아 있으면 안 된다.
    QVERIFY2(hasNoAutosaveTimer(window), "20초 자동 저장 타이머가 아직 살아 있다");
    bool saveDuringRead = true;
    const auto connection = connect(QgsProject::instance(), &QgsProject::readProject,
        &window, [&](const QDomDocument&) {
          if (invoked) return;
          invoked = true;
          saveDuringRead = saveNow(window);
          nestedOpened = window.openSurveyGpkg(first);
        });
    const bool opened = window.openSurveyGpkg(second);
    disconnect(connection);
    QVERIFY(invoked);
    QVERIFY2(!saveDuringRead, "프로젝트를 읽는 중에는 저장이 끼어들면 안 된다");
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
    const QString path = makeSurvey(QStringLiteral("유지할조사_") + QString::fromLatin1(QTest::currentDataTag()));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QStringList layerIds = QgsProject::instance()->mapLayers().keys();
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
    QVERIFY(saveNow(window));
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
    QVERIFY(openWithAnswer(window, second, QMessageBox::Discard));
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
  void open_unsavedWorkRespectsAnswer_data() {
    QTest::addColumn<int>("answer");
    QTest::addColumn<bool>("vetoCommit");
    QTest::newRow("cancel") << int(QMessageBox::Cancel) << false;
    QTest::newRow("save-fails") << int(QMessageBox::Save) << true;
    QTest::newRow("discard") << int(QMessageBox::Discard) << false;
    QTest::newRow("save") << int(QMessageBox::Save) << false;
  }
  void open_unsavedWorkRespectsAnswer() {
    QFETCH(int, answer);
    QFETCH(bool, vetoCommit);
    const QString suffix = QString::number(answer) + QString::number(vetoCommit);
    const QString source = makeSurvey(QStringLiteral("열기전현재_") + suffix);
    const QString next = makeSurvey(QStringLiteral("열기전다음_") + suffix);
    QVERIFY(!source.isEmpty() && !next.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(oldLayer && oldLayer->startEditing());
    const QgsFeatureId fid = *oldLayer->allFeatureIds().constBegin();
    const QgsGeometry original = oldLayer->getFeature(fid).geometry();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190010, 560011, 190121, 560122));
    QVERIFY(oldLayer->changeGeometry(fid, pending));
    const QStringList layerIds = project->mapLayers().keys();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    if (vetoCommit) oldLayer->setAllowCommit(false);
    bool prompted = false;
    const bool opened = openWithAnswer(window, next, QMessageBox::StandardButton(answer), &prompted);
    if (oldLayer) oldLayer->setAllowCommit(true);
    QVERIFY(prompted);
    const bool shouldOpen = answer != int(QMessageBox::Cancel) && !vetoCommit;
    QCOMPARE(opened, shouldOpen);
    if (!shouldOpen) {
      QVERIFY(oldLayer && oldLayer->isEditable() && oldLayer->isModified());
      QCOMPARE(project->mapLayers().keys(), layerIds);
      QVERIFY(oldLayer->getFeature(fid).geometry().equals(pending));
      QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    } else {
      QVERIFY(oldLayer.isNull());
      auto* current = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
      QVERIFY(current && current->isValid());
      QCOMPARE(QFileInfo(current->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
               QFileInfo(next).absoluteFilePath());
      QgsVectorLayer stored(source + QStringLiteral("|layername=survey_area"), QStringLiteral("저장본"), QStringLiteral("ogr"));
      QVERIFY(stored.isValid());
      QVERIFY(stored.getFeature(fid).geometry().equals(answer == int(QMessageBox::Save) ? pending : original));
    }
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
    QVERIFY(selectSaveAs(window, target));
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
  void newSurvey_existingNamePreservesUnsavedCurrentSurvey() {
    const QString name = QStringLiteral("새조사실패보존");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry changed = QgsGeometry::fromRect(QgsRectangle(190010, 560020, 190110, 560120));
    QVERIFY(layer->changeGeometry(fid, changed));
    const QStringList layerIds = project->mapLayers().keys();
    const QString projectTitle = project->title();
    const QString windowTitle = window.windowTitle();
    const QByteArray before = QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256);
    const QString qgz = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    const QByteArray companionBefore = contents(qgz);
    QTimer choose;
    bool discarded = false;
    bool folderChosen = false;
    int nameSubmissions = 0;
    int folderSubmissions = 0;
    QStringList selectedFolders;
    QString submittedName;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* question = qobject_cast<QMessageBox*>(dialog)) {
        if (question->standardButtons().testFlag(QMessageBox::Discard)) {
          discarded = true;
          question->button(QMessageBox::Discard)->click();
        } else question->accept();
      } else if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        ++folderSubmissions;
        const QFileInfo desired(m_files.path());
        folder->setDirectory(desired.absolutePath());
        folder->selectFile(desired.fileName());
        if (auto* pathEdit = folder->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
          pathEdit->setText(desired.fileName());
        selectedFolders = folder->selectedFiles();
        folderChosen = selectedFolders.size() == 1 &&
            QFileInfo(selectedFolders.first()).canonicalFilePath() == desired.canonicalFilePath();
        qInfo() << "New survey selected folders:" << selectedFolders << "matches fixture:" << folderChosen;
        choose.stop();
        if (!folderChosen) { folder->reject(); return; }
        QMetaObject::invokeMethod(folder, "accept", Qt::DirectConnection);
      } else if (dialog->windowTitle() == QStringLiteral("새 조사")) {
        ++nameSubmissions;
        auto* nameEdit = dialog->findChild<QLineEdit*>();
        nameEdit->setText(name);
        submittedName = nameEdit->text();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    qInfo() << "New survey dialog state:" << discarded << nameSubmissions << folderSubmissions << submittedName;
    QVERIFY(discarded && folderChosen);
    QCOMPARE(nameSubmissions, 1);
    QCOMPARE(folderSubmissions, 1);
    QCOMPARE(submittedName, name);
    QCOMPARE(QFileInfo(selectedFolders.first()).canonicalFilePath(), QFileInfo(m_files.path()).canonicalFilePath());
    QCOMPARE(project->mapLayers().keys(), layerIds);
    QCOMPARE(project->title(), projectTitle);
    QCOMPARE(window.windowTitle(), windowTitle);
    QVERIFY(layer->isEditable() && layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(changed));
    QCOMPARE(QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256), before);
    QCOMPARE(contents(qgz), companionBefore);
  }
  void closeSave_preservesMemoryReferenceVectorOnReopen() {
    const QString path = makeSurvey(QStringLiteral("닫기저장참조벡터"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("현장참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature feature(reference->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210000, 550000)));
    feature.setAttribute(QStringLiteral("note"), QStringLiteral("다음날 다시 볼 지점"));
    QVERIFY(reference->addFeature(feature));
    QTimer choose;
    bool prompted = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(QMessageBox::Save)) {
          prompted = true;
          question->button(QMessageBox::Save)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    const bool closed = window.close();
    choose.stop();
    QVERIFY(prompted && closed);
    QVERIFY(window.openSurveyGpkg(path));
    reference = qobject_cast<QgsVectorLayer*>(project->mapLayer(referenceId));
    QVERIFY(reference && reference->isValid());
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restored = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restored.geometry().asPoint(), QgsPointXY(210000, 550000));
    QCOMPARE(restored.attribute(QStringLiteral("note")).toString(), QStringLiteral("다음날 다시 볼 지점"));
    QCOMPARE(QFileInfo(reference->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(path).absoluteFilePath());
  }
  void saveAs_embeddedFailureRestoresSourcesAndRemainsUnsaved() {
    const QString path = makeSurvey(QStringLiteral("사본실패복귀"));
    QVERIFY(!path.isEmpty());
    // The copied database retains these triggers, rejecting only workspace writes.
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_insert BEFORE INSERT ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_update BEFORE UPDATE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_delete BEFORE DELETE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString source = layer->source();
    const QString projectFile = project->fileName();
    const QString projectHome = project->presetHomePath();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("실패후에도남을참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature point(reference->fields());
    point.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210010, 550020)));
    point.setAttribute(QStringLiteral("note"), QStringLiteral("보존할 메모"));
    QVERIFY(reference->addFeature(point));
    project->setTitle(QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    project->setDirty(true);
    const QString target = m_files.filePath(QStringLiteral("실패할사본.gpkg"));
    QVERIFY(selectSaveAs(window, target));
    QVERIFY(QFile::exists(target));
    QVERIFY(layer->isValid());
    QCOMPARE(layer->source(), source);
    QCOMPARE(project->fileName(), projectFile);
    QCOMPARE(project->presetHomePath(), projectHome);
    QVERIFY(project->isDirty());
    QCOMPARE(project->mapLayer(referenceId), reference);
    QVERIFY(reference->isValid() && reference->isEditable());
    QCOMPARE(reference->providerType(), QStringLiteral("memory"));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restoredPoint = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restoredPoint.geometry().asPoint(), QgsPointXY(210010, 550020));
    QCOMPARE(restoredPoint.attribute(QStringLiteral("note")).toString(), QStringLiteral("보존할 메모"));
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_insert"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_update"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_delete"));
    QVERIFY(saveNow(window));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    QVERIFY(!project->isDirty());
  }
  void saveAs_existingDestinationIsNotOverwritten_data() {
    QTest::addColumn<bool>("companionOnly");
    QTest::newRow("existing-gpkg") << false;
    QTest::newRow("existing-qgz") << true;
  }
  void saveAs_existingDestinationIsNotOverwritten() {
    QFETCH(bool, companionOnly);
    const QString source = makeSurvey(QStringLiteral("덮어쓰기방지원본_%1").arg(companionOnly));
    QVERIFY(!source.isEmpty());
    const QString target = m_files.filePath(QStringLiteral("기존자료_%1.gpkg").arg(companionOnly));
    const QString existing = companionOnly
        ? QFileInfo(target).dir().filePath(QFileInfo(target).completeBaseName() + QStringLiteral(".qgz"))
        : target;
    const QString sourceQgz = QFileInfo(source).dir().filePath(QFileInfo(source).completeBaseName() + QStringLiteral(".qgz"));
    QVERIFY(QFile::copy(companionOnly ? sourceQgz : source, existing));
    const QByteArray before = QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256);
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QString originalSource = layer->source();
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190001, 560002, 190111, 560112));
    QVERIFY(layer->changeGeometry(fid, pending));
    QVERIFY(selectSaveAs(window, target));
    QCOMPARE(QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256), before);
    QCOMPARE(layer->source(), originalSource);
    QVERIFY(layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(pending));
    if (companionOnly) QVERIFY(!QFileInfo::exists(target));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("이름"))) warned = true;
    QVERIFY(warned);
  }
  void save_qgisExceptionStaysInsideSlot_data() {
    QTest::addColumn<bool>("saveAs");
    QTest::newRow("persist") << false;
    QTest::newRow("save-as") << true;
  }
  void save_qgisExceptionStaysInsideSlot() {
    QFETCH(bool, saveAs);
    const QString source = makeSurvey(QStringLiteral("예외보존_%1").arg(saveAs));
    QVERIFY(!source.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString originalSource = layer->source();
    const QString originalProjectFile = project->fileName();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    project->setTitle(QStringLiteral("예외 뒤에도 남을 제목"));
    project->setDirty(true);
    bool injected = false;
    const auto connection = connect(project, &QgsProject::writeProject, &window,
        [&](QDomDocument&) {
          injected = true;
          throw QgsException(QStringLiteral("synthetic non-std write exception"));
        }, Qt::DirectConnection);
    bool escaped = false;
    bool result = false;
    try {
      result = saveAs ? selectSaveAs(window, m_files.filePath(QStringLiteral("예외사본.gpkg")))
                      : saveNow(window);
    } catch (...) {
      escaped = true;
    }
    disconnect(connection);
    QVERIFY2(!escaped, "QGIS exceptions must not escape the save slot into the GUI event loop");
    QVERIFY(injected);
    if (saveAs) QVERIFY(result);
    else QVERIFY(!result);
    QCOMPARE(layer->source(), originalSource);
    QCOMPARE(project->fileName(), originalProjectFile);
    QVERIFY(project->isDirty());
    QCOMPARE(project->title(), QStringLiteral("예외 뒤에도 남을 제목"));
    QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    QVERIFY(warned);
    QVERIFY(saveNow(window));
  }
  void save_companionFailureKeepsEmbeddedWorkspaceAndWarns() {
    const QString name = QStringLiteral("보조사본실패");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QString companion = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    QVERIFY(QFile::remove(companion));
    QVERIFY(QDir().mkdir(companion)); // A directory cannot be atomically replaced by a QGZ file.
    auto* project = QgsProject::instance();
    project->setTitle(QStringLiteral("내장 구성은 저장됨"));
    QVERIFY(saveNow(window));
    QVERIFY(QFileInfo(companion).isDir());
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("내장 구성은 저장됨"));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items()) {
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QLatin1String("QGZ")) &&
          item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    }
    QVERIFY2(warned, "A failed companion write must show an actionable warning even after GPKG success");
    QVERIFY(!project->isDirty());
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

static QByteArray localCadastralCapabilities() {
  return QByteArray(R"(<WMS_Capabilities version="1.3.0" xmlns="http://www.opengis.net/wms" xmlns:xlink="http://www.w3.org/1999/xlink">
<Service><Name>WMS</Name><Title>Offline save/open fixture</Title></Service>
<Capability><Request>
<GetCapabilities><Format>text/xml</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetCapabilities>
<GetMap><Format>image/png</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetMap>
</Request><Layer><Title>Local cadastral fixture</Title>
<CRS>EPSG:3857</CRS><CRS>EPSG:5186</CRS><CRS>EPSG:5187</CRS><CRS>EPSG:4326</CRS>
<EX_GeographicBoundingBox><westBoundLongitude>124</westBoundLongitude><eastBoundLongitude>132</eastBoundLongitude><southBoundLatitude>32</southBoundLatitude><northBoundLatitude>40</northBoundLatitude></EX_GeographicBoundingBox>
<BoundingBox CRS="EPSG:3857" minx="13500000" miny="3500000" maxx="14900000" maxy="4900000"/>
<BoundingBox CRS="EPSG:5186" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:5187" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:4326" minx="32" miny="124" maxx="40" maxy="132"/>
<Layer queryable="0"><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title><Style><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title></Style></Layer>
<Layer queryable="0"><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title><Style><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title></Style></Layer>
</Layer></Capability></WMS_Capabilities>)");
}

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  // GDAL WMS uses libcurl outside QGIS's request preprocessor. Keep its remote
  // requests offline too; these options are scoped to this test process.
  CPLSetConfigOption("GDAL_HTTP_PROXY", "127.0.0.1:1");
  CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    // Match the installed field font: offscreen Qt does not discover it itself.
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      if (QFontDatabase::addApplicationFont(path) < 0) {
        qCritical().noquote() << "Could not load the installed QA font:" << path;
        return 1;
      }
    }
  }
#endif
  QTemporaryDir settings;
  const QString capabilitiesPath = settings.filePath(QStringLiteral("cadastral-capabilities.xml"));
  const QString mapPath = settings.filePath(QStringLiteral("white-map.png"));
  QFile capabilities(capabilitiesPath);
  const QByteArray capabilitiesXml = localCadastralCapabilities();
  if (!capabilities.open(QIODevice::WriteOnly) || capabilities.write(capabilitiesXml) != capabilitiesXml.size())
    return 1;
  capabilities.close();
  QImage mapImage(512, 512, QImage::Format_RGB32);
  mapImage.fill(Qt::white);
  if (!mapImage.save(mapPath)) return 1;
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  // Save/open fixtures use local rasters. Incidental basemap refreshes must not
  // depend on external servers; network behavior has its own dedicated suites.
  const QUrl capabilitiesUrl = QUrl::fromLocalFile(capabilitiesPath);
  const QUrl mapUrl = QUrl::fromLocalFile(mapPath);
  const QString networkIsolation = QgsNetworkAccessManager::setRequestPreprocessor([capabilitiesUrl, mapUrl](QNetworkRequest* request) {
    if (!request) return;
    const QUrl url = request->url();
    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    if ((scheme == QLatin1String("http") || scheme == QLatin1String("https")) &&
        host != QLatin1String("localhost") && host != QLatin1String("127.0.0.1") &&
        host != QLatin1String("::1")) {
      bool capabilitiesRequest = false;
      for (const auto& item : QUrlQuery(url).queryItems())
        if (item.first.compare(QLatin1String("request"), Qt::CaseInsensitive) == 0 &&
            item.second.compare(QLatin1String("GetCapabilities"), Qt::CaseInsensitive) == 0)
          capabilitiesRequest = true;
      // Valid local capabilities avoid the GDAL failure fallback. The entire URL
      // is replaced so credentials from remote query/path stay private.
      request->setUrl(capabilitiesRequest ? capabilitiesUrl : mapUrl);
    }
  });
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  s_testSettingsPath = settings.path();
  KaTheme::apply(&app);
  TestSaveOpen tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  QgsNetworkAccessManager::removeRequestPreprocessor(networkIsolation);
  return result;
}

#include "test_save_open.moc"
