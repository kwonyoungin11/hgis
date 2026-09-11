#include <QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QDir>
#include <QUuid>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <qgsapplication.h>
#include <qgscolorrampshader.h>
#include <qgscoordinatetransform.h>
#include <qgshillshaderenderer.h>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsprojectviewsettings.h>
#include <qgsmapcanvas.h>
#include <qgsrasterlayer.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <memory>
#include <vector>
#include "core/DemColorRampLegend.h"
#include "core/DemPresentation.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include "app/KaDemClassDialog.h"

namespace {
QByteArray digest(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) return {};
  return hash.result();
}

bool writeElevation(const QString& path) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      driver->Create(path.toUtf8().constData(), 64, 32, 1, GDT_Float32, nullptr), GDALClose);
  if (!dataset) return false;
  OGRSpatialReference crs;
  double transform[] = {128.70, .0002, 0., 37.72, 0., -.0002};
  if (crs.importFromEPSG(4326) != OGRERR_NONE || dataset->SetSpatialRef(&crs) != CE_None ||
      dataset->SetGeoTransform(transform) != CE_None) return false;
  auto* band = dataset->GetRasterBand(1);
  if (band->SetNoDataValue(-9999.) != CE_None) return false;
  std::vector<float> values(64 * 32);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      values[static_cast<size_t>(y) * 64 + x] = x < 48 ? float(x * 10) : -9999.f;
  return band->RasterIO(GF_Write, 0, 0, 64, 32, values.data(), 64, 32,
                        GDT_Float32, 0, 0) == CE_None;
}

QgsSingleBandPseudoColorRenderer* colorRenderer(QgsRasterLayer* layer) {
  return dynamic_cast<QgsSingleBandPseudoColorRenderer*>(layer->renderer());
}

QColor elevationColor(QgsRasterLayer* layer, double value) {
  auto* renderer = colorRenderer(layer);
  if (!renderer || !renderer->shader()) return {};
  int r = 0, g = 0, b = 0, a = 0;
  if (!renderer->shader()->shade(value, &r, &g, &b, &a)) return {};
  return QColor(r, g, b, a);
}

QgsRasterLayer* firstRaster(QgsProject& project, const QString& name) {
  const auto layers = project.mapLayersByName(name);
  return layers.isEmpty() ? nullptr : qobject_cast<QgsRasterLayer*>(layers.first());
}
}

class DemPresentationTest : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_files;
  QString m_source;
  QByteArray m_originalHash;
  QStringList m_ownedCaches;

  QgsRasterLayer* addDem(QgsProject& project) {
    auto layer = std::make_unique<QgsRasterLayer>(m_source, QStringLiteral("DEM"), QStringLiteral("gdal"));
    if (!layer->isValid()) return nullptr;
    auto* result = layer.get();
    if (!project.addMapLayer(result)) return nullptr;
    layer.release();
    return result;
  }

  QgsRasterLayer* ensure(QgsProject& project, QgsRasterLayer* dem) {
    auto* shade = LayerOps::ensureDemRelief(&project, dem);
    // This test's unique temporary source gives its VRT a unique cache signature.
    // Never enumerate or clear shared application caches.
    if (shade && shade->source() != m_source && !m_ownedCaches.contains(shade->source()))
      m_ownedCaches.append(shade->source());
    return shade;
  }

private slots:
  void initTestCase() {
    QVERIFY(m_files.isValid());
    GDALAllRegister();
    m_source = m_files.filePath(QStringLiteral("elevation.tif"));
    QVERIFY(writeElevation(m_source));
    m_originalHash = digest(m_source);
    QVERIFY(!m_originalHash.isEmpty());
  }

  void cleanup() {
    QCOMPARE(digest(m_source), m_originalHash);
    QVERIFY(!QFileInfo::exists(m_source + QStringLiteral(".aux.xml")));
  }

  void cleanupTestCase() {
    for (const auto& path : m_ownedCaches) {
      if (QFileInfo::exists(path)) QVERIFY2(QFile::remove(path), qPrintable(path));
    }
  }

  void presetsAndFailedViewportPreserveDisplay() {
    QgsRasterLayer dem(m_source, QStringLiteral("DEM"), QStringLiteral("gdal"));
    QVERIFY(dem.isValid());
    QVERIFY(DemPresentation::apply(&dem));
    auto* renderer = colorRenderer(&dem);
    QVERIFY(renderer);
    QCOMPARE(renderer->classificationMin(), 0.);
    QCOMPARE(renderer->classificationMax(), 2000.);
    const QColor national50 = elevationColor(&dem, 50.);
    QVERIFY(national50.isValid());
    QVERIFY(DemPresentation::apply(&dem, QStringLiteral("lowland")));
    QVERIFY(elevationColor(&dem, 50.) != national50);
    QCOMPARE(colorRenderer(&dem)->classificationMax(), 2000.);
    QVERIFY(DemPresentation::apply(&dem, QStringLiteral("national")));
    QCOMPARE(elevationColor(&dem, 50.), national50);

    // Pixel centers 10..25 are elevations 100..250 m; no resampling/statistical sampling.
    const QgsRectangle subset(128.702, 37.7136, 128.7052, 37.72);
    QVERIFY(DemPresentation::apply(&dem, QStringLiteral("viewport"), subset));
    renderer = colorRenderer(&dem);
    QVERIFY(renderer);
    QVERIFY(renderer->classificationMin() >= 90. && renderer->classificationMin() <= 110.);
    QVERIFY(renderer->classificationMax() >= 240. && renderer->classificationMax() <= 260.);
    const QColor before = elevationColor(&dem, 150.);
    // The far-right strip consists exclusively of NoData values.
    const QgsRectangle noData(128.71, 37.714, 128.7126, 37.7198);
    QVERIFY(!DemPresentation::apply(&dem, QStringLiteral("viewport"), noData));
    QCOMPARE(colorRenderer(&dem), renderer);
    QCOMPARE(elevationColor(&dem, 150.), before);
    QVERIFY(!DemPresentation::apply(&dem, QStringLiteral("viewport"), QgsRectangle(0., 0., 1., 1.)));
    QCOMPARE(colorRenderer(&dem), renderer);
  }

  void actualDialogAppliesPresetsAndReliefAndClosesWithLayer() {
    // The product dialog uses the application project, just as MainWindow does.
    auto* project = QgsProject::instance();
    QVERIFY(project->mapLayers().isEmpty());
    const auto cleanupProject = qScopeGuard([project] { project->clear(); });
    project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* dem = addDem(*project); QVERIFY(dem);
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project->crs());
    canvas.resize(220, 550);
    canvas.show();
    QCoreApplication::processEvents();
    const QgsRectangle subset(128.702, 37.7136, 128.7052, 37.72);
    const QgsCoordinateTransform toCanvas(dem->crs(), project->crs(), project);
    canvas.setExtent(toCanvas.transformBoundingBox(subset));
    KaDemClassDialog dialog(dem, nullptr, &canvas);
    dialog.show();
    QCoreApplication::processEvents();
    auto* preset = dialog.findChild<QComboBox*>(QStringLiteral("demPreset"));
    auto* relief = dialog.findChild<QCheckBox*>(QStringLiteral("demReliefEnabled"));
    auto* exaggeration = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("demExaggeration"));
    auto* strength = dialog.findChild<QSpinBox*>(QStringLiteral("demReliefStrength"));
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(preset && relief && exaggeration && strength && buttons);
    auto* apply = buttons->button(QDialogButtonBox::Apply); QVERIFY(apply);
    const auto choosePreset = [preset](const QString& value) {
      const int index = preset->findData(value);
      if (index < 0) return false;
      preset->setFocus();
      QTest::keyClick(preset, Qt::Key_Home);
      for (int step = 0; step < index; ++step) QTest::keyClick(preset, Qt::Key_Down);
      return preset->currentData().toString() == value;
    };
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const auto capture = [&dialog, &output](const QString& name) {
      return output.isEmpty() || (QDir().mkpath(output) &&
          dialog.grab().save(QDir(output).filePath(name + QStringLiteral(".png"))));
    };
    const auto clickReliefIndicator = [relief] {
      // QFormLayout stretches the checkbox across the row, but its blank
      // trailing area is outside QCheckBox's clickable indicator/text area.
      QStyleOptionButton option;
      option.initFrom(relief);
      option.text = relief->text();
      option.state |= relief->isChecked() ? QStyle::State_On : QStyle::State_Off;
      const QRect indicator = relief->style()->subElementRect(QStyle::SE_CheckBoxIndicator,
                                                               &option, relief);
      if (indicator.isEmpty() || !relief->rect().contains(indicator.center())) return false;
      QTest::mouseClick(relief, Qt::LeftButton, Qt::NoModifier, indicator.center());
      return true;
    };

    QCOMPARE(preset->currentData().toString(), QStringLiteral("national"));
    QVERIFY(choosePreset(QStringLiteral("national")));
    QTest::mouseClick(apply, Qt::LeftButton);
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(), QStringLiteral("national"));
    QVERIFY(colorRenderer(dem));
    QCOMPARE(colorRenderer(dem)->classificationMin(), 0.);
    QCOMPARE(colorRenderer(dem)->classificationMax(), 2000.);
    const QColor national50 = elevationColor(dem, 50.);
    auto* shade = firstRaster(*project, QStringLiteral("지형 음영"));
    QVERIFY(shade && shade->isValid());
    if (shade->source() != m_source && !m_ownedCaches.contains(shade->source()))
      m_ownedCaches.append(shade->source());
    auto* shadeNode = project->layerTreeRoot()->findLayer(shade->id()); QVERIFY(shadeNode);
    QVERIFY(shadeNode->itemVisibilityChecked());
    QVERIFY(capture(QStringLiteral("dem-dialog-national")));

    QVERIFY(relief->isChecked());
    QSignalSpy reliefToggled(relief, &QCheckBox::toggled);
    QVERIFY(clickReliefIndicator());
    QVERIFY(!relief->isChecked());
    QCOMPARE(reliefToggled.count(), 1);
    QTest::mouseClick(apply, Qt::LeftButton);
    QVERIFY(!dem->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled")).toBool());
    QVERIFY(!shadeNode->itemVisibilityChecked());
    QVERIFY(capture(QStringLiteral("dem-dialog-relief-off")));

    QVERIFY(choosePreset(QStringLiteral("lowland")));
    QVERIFY(clickReliefIndicator());
    QVERIFY(relief->isChecked());
    QCOMPARE(reliefToggled.count(), 2);
    exaggeration->setFocus(); exaggeration->selectAll();
    QTest::keyClicks(exaggeration, "1.7");
    QTest::keyClick(exaggeration, Qt::Key_Tab);
    strength->setFocus(); strength->selectAll();
    QTest::keyClicks(strength, "45");
    QTest::keyClick(strength, Qt::Key_Tab);
    QTest::mouseClick(apply, Qt::LeftButton);
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(), QStringLiteral("lowland"));
    QVERIFY(elevationColor(dem, 50.) != national50);
    QVERIFY(shadeNode->itemVisibilityChecked());
    auto* hillshade = dynamic_cast<QgsHillshadeRenderer*>(shade->renderer()); QVERIFY(hillshade);
    QCOMPARE(hillshade->zFactor(), 1.7);
    QCOMPARE(shade->opacity(), .45);
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_z_factor")).toDouble(), 1.7);
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_relief_strength")).toDouble(), .45);
    QVERIFY(capture(QStringLiteral("dem-dialog-lowland")));

    // Reassert a central subset after map/layout events. The data's complete
    // valid range is 0..470 m; this viewport must exclude both outside edges.
    canvas.setExtent(toCanvas.transformBoundingBox(subset));
    QVERIFY(choosePreset(QStringLiteral("viewport")));
    QTest::mouseClick(apply, Qt::LeftButton);
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(), QStringLiteral("viewport"));
    QVERIFY(colorRenderer(dem)->classificationMin() >= 80.);
    QVERIFY(colorRenderer(dem)->classificationMax() <= 280.);
    QVERIFY(colorRenderer(dem)->classificationMin() < colorRenderer(dem)->classificationMax());
    QVERIFY(capture(QStringLiteral("dem-dialog-viewport")));
    QVERIFY(dialog.isVisible());
    QSignalSpy rejected(&dialog, &QDialog::rejected);
    canvas.setRenderFlag(false);
    canvas.setLayers({});
    project->removeMapLayer(dem->id());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTRY_VERIFY(!dialog.isVisible());
    QCOMPARE(rejected.count(), 1);
  }

  void optionalNativePreviewUsesPersistentQaData() {
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const QString realDir = qEnvironmentVariable("KA_DEM_QA_REAL_DIR");
    if (output.isEmpty() || realDir.isEmpty())
      QSKIP("Native preview requires KA_HGIS_QA_OUTPUT_DIR and KA_DEM_QA_REAL_DIR.");
    const QString original = QDir(realDir).filePath(QStringLiteral("gangneung-coast.tif"));
    if (!QFileInfo::exists(original)) QSKIP("The optional real coastal DEM fixture is unavailable.");
    const QByteArray originalHash = digest(original); QVERIFY(!originalHash.isEmpty());
    const QString folder = QDir(output).filePath(QStringLiteral("native-preview-") +
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(folder));
    const QString source = QDir(folder).filePath(QStringLiteral("gangneung-coast.tif"));
    QVERIFY(QFile::copy(original, source));
    QString error;
    const QString name = QStringLiteral("DEM-native-preview");
    const QString gpkg = SurveyProjectFactory::createNewSurvey(folder, name, &error,
                                                               QStringLiteral("EPSG:5187"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    const QString qgz = QDir(folder).filePath(name + QStringLiteral(".qgz"));
    {
      QgsProject project;
      project.setTitle(name);
      project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
      auto dem = std::make_unique<QgsRasterLayer>(source, QStringLiteral("DEM"), QStringLiteral("gdal"));
      QVERIFY(dem->isValid());
      QVERIFY(DemPresentation::apply(dem.get(), QStringLiteral("national")));
      LayerOps::markReferenceLayer(dem.get());
      auto* layer = dem.get();
      QVERIFY(project.addMapLayer(layer)); dem.release();
      // This is a persistent QA artifact: its VRT must survive test cleanup.
      auto* shade = LayerOps::ensureDemRelief(&project, layer); QVERIFY(shade && shade->isValid());
      const QgsCoordinateTransform transform(layer->crs(), project.crs(), &project);
      const QgsReferencedRectangle extent(transform.transformBoundingBox(layer->extent()), project.crs());
      project.viewSettings()->setDefaultViewExtent(extent);
      QVERIFY(project.write(qgz));
      QVERIFY2(SurveyStorage::writeEmbedded(&project, gpkg, &error), qPrintable(error));
    }
    QgsProject reopened;
    QVERIFY2(SurveyStorage::readEmbedded(&reopened, gpkg, nullptr, &error), qPrintable(error));
    auto* dem = firstRaster(reopened, QStringLiteral("DEM")); QVERIFY(dem && dem->isValid());
    auto* shade = firstRaster(reopened, QStringLiteral("지형 음영")); QVERIFY(shade && shade->isValid());
    QVERIFY(DemPresentation::restore(dem));
    QCOMPARE(colorRenderer(dem)->classificationMax(), 2000.);
    QCOMPARE(digest(original), originalHash);
    QCOMPARE(digest(source), originalHash);
    qInfo().noquote() << "Native preview survey:" << QDir::toNativeSeparators(gpkg);
  }

  void saveReopenRestoresColorModeAndLegend_data() {
    QTest::addColumn<QString>("preset");
    for (const auto* name : {"national", "lowland", "viewport"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }

  void saveReopenRestoresColorModeAndLegend() {
    QFETCH(QString, preset);
    const QString path = m_files.filePath(preset + QStringLiteral(".qgz"));
    QColor expected;
    double minimum = 0., maximum = 0.;
    {
      QgsProject project;
      project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
      auto* dem = addDem(project); QVERIFY(dem);
      QVERIFY(DemPresentation::apply(dem, preset, dem->extent()));
      expected = elevationColor(dem, 100.);
      minimum = colorRenderer(dem)->classificationMin();
      maximum = colorRenderer(dem)->classificationMax();
      dem->setOpacity(.73);
      QVERIFY(project.write(path));
    }
    QgsProject reopened;
    QVERIFY(reopened.read(path));
    auto* dem = firstRaster(reopened, QStringLiteral("DEM")); QVERIFY(dem);
    QVERIFY(DemPresentation::restore(dem));
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(), preset);
    QCOMPARE(elevationColor(dem, 100.), expected);
    QCOMPARE(colorRenderer(dem)->classificationMin(), minimum);
    QCOMPARE(colorRenderer(dem)->classificationMax(), maximum);
    QVERIFY(qAbs(dem->opacity() - .73) < .000001);
    QVERIFY(dynamic_cast<DemColorRampLegend*>(dem->legend()));
  }

  void metricReliefAndManualVisibilitySurviveReopen_data() {
    QTest::addColumn<QString>("crs");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }

  void metricReliefAndManualVisibilitySurviveReopen() {
    QFETCH(QString, crs);
    const QString path = m_files.filePath(crs.mid(5) + QStringLiteral("-shade.qgz"));
    const QString hiddenPath = m_files.filePath(crs.mid(5) + QStringLiteral("-hidden-shade.qgz"));
    {
      QgsProject project; project.setCrs(QgsCoordinateReferenceSystem(crs));
      auto* dem = addDem(project); QVERIFY(dem);
      auto* shade = ensure(project, dem); QVERIFY(shade); QVERIFY(shade->isValid());
      QCOMPARE(shade->crs().authid(), crs);
      QCOMPARE(shade->crs().mapUnits(), Qgis::DistanceUnit::Meters);
      QVERIFY(shade->source() != m_source);
      auto* hillshade = dynamic_cast<QgsHillshadeRenderer*>(shade->renderer()); QVERIFY(hillshade);
      QCOMPARE(hillshade->zFactor(), 1.);
      QCOMPARE(shade->blendMode(), QPainter::CompositionMode_Multiply);
      QCOMPARE(shade->opacity(), .30);
      auto* node = project.layerTreeRoot()->findLayer(shade->id()); QVERIFY(node);
      node->setItemVisibilityChecked(false);
      QVERIFY(!dem->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool());
      auto* demNode = project.layerTreeRoot()->findLayer(dem->id()); QVERIFY(demNode);
      demNode->setItemVisibilityChecked(false); demNode->setItemVisibilityChecked(true);
      QCOMPARE(ensure(project, dem), shade);
      QVERIFY(!node->itemVisibilityChecked());
      dem->setCustomProperty(QStringLiteral("ka_hgis/dem_z_factor"), 1.7);
      dem->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_strength"), .45);
      QVERIFY(project.write(hiddenPath));
      const QString shadeId = shade->id();
      project.removeMapLayer(shadeId);
      QVERIFY(!ensure(project, dem)); // Disabled DEM must not recreate a visible shade.
      QVERIFY(project.write(path));
    }
    {
      QgsProject reopened; QVERIFY(reopened.read(hiddenPath));
      auto* dem = firstRaster(reopened, QStringLiteral("DEM")); QVERIFY(dem);
      auto* shade = ensure(reopened, dem); QVERIFY(shade);
      auto* node = reopened.layerTreeRoot()->findLayer(shade->id()); QVERIFY(node);
      QVERIFY(!node->itemVisibilityChecked());
      auto* renderer = dynamic_cast<QgsHillshadeRenderer*>(shade->renderer()); QVERIFY(renderer);
      QCOMPARE(renderer->zFactor(), 1.7); QCOMPARE(shade->opacity(), .45);
    }
    QgsProject reopened; QVERIFY(reopened.read(path));
    auto* dem = firstRaster(reopened, QStringLiteral("DEM")); QVERIFY(dem);
    QVERIFY(!dem->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool());
    QVERIFY(!ensure(reopened, dem));
  }

  void deletedCacheRebuildsInvalidSavedShade() {
    const QString projectPath = m_files.filePath(QStringLiteral("deleted-cache.qgz"));
    QString cache;
    {
      QgsProject project; project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
      auto* dem = addDem(project); QVERIFY(dem);
      auto* shade = ensure(project, dem); QVERIFY(shade);
      cache = shade->source();
      QVERIFY(m_ownedCaches.contains(cache)); QVERIFY(cache != m_source);
      QVERIFY(project.write(projectPath));
    }
    QVERIFY(QFile::remove(cache)); // Only the VRT generated from this test fixture.
    QgsProject reopened; QVERIFY(reopened.read(projectPath));
    auto* dem = firstRaster(reopened, QStringLiteral("DEM")); QVERIFY(dem); QVERIFY(dem->isValid());
    auto* invalid = firstRaster(reopened, QStringLiteral("지형 음영")); QVERIFY(invalid);
    QVERIFY(!invalid->isValid());
    auto* recovered = ensure(reopened, dem);
    QVERIFY(recovered); QCOMPARE(recovered, invalid); QVERIFY(recovered->isValid());
    QCOMPARE(recovered->source(), cache); QVERIFY(QFileInfo::exists(cache));
  }

  void replacingDemRebindsShadeVisibility() {
    QgsProject project; project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* original = addDem(project); QVERIFY(original);
    auto* shade = ensure(project, original); QVERIFY(shade);
    const QString shadeId = shade->id();
    project.removeMapLayer(original->id());
    auto* replacement = addDem(project); QVERIFY(replacement);
    QCOMPARE(ensure(project, replacement), shade);
    auto* node = project.layerTreeRoot()->findLayer(shadeId); QVERIFY(node);
    node->setItemVisibilityChecked(false);
    QVERIFY(!replacement->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool());
    node->setItemVisibilityChecked(true);
    QVERIFY(replacement->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), false).toBool());
    replacement->setCustomProperty(QStringLiteral("ka_hgis/dem_z_factor"), 1.7);
    replacement->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_strength"), .45);
    QCOMPARE(ensure(project, replacement), shade);
    auto* renderer = dynamic_cast<QgsHillshadeRenderer*>(shade->renderer()); QVERIFY(renderer);
    QCOMPARE(renderer->zFactor(), 1.7); QCOMPARE(shade->opacity(), .45);
  }
};

int main(int argc, char** argv) {
  CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
      if (QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file)) < 0) return 2;
  }
#endif
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  app.setApplicationName(QStringLiteral("ka-hgis-dem-presentation-tests"));
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  { DemPresentationTest test; result = QTest::qExec(&test, argc, argv); }
  QgsApplication::exitQgis();
  return result;
}
#include "test_dem_presentation.moc"
