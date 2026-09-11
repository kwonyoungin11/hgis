#include <QtTest>
#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <qgsapplication.h>
#include <qgscolorrampshader.h>
#include <qgscoordinatetransform.h>
#include <qgslayertreelayer.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayertreeview.h>
#include <qgslayoutexporter.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutpagecollection.h>
#include <qgsmaplayerlegend.h>
#include <qgsmaprendererparalleljob.h>
#include <qgsmapsettings.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "core/LayerOps.h"
#include "core/LayoutService.h"

namespace {
bool writeFixture(const QString& path, double low, double high) {
  GDALAllRegister();
  auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      driver->Create(path.toUtf8().constData(), 200, 160, 1, GDT_Float32, nullptr), GDALClose);
  if (!dataset) return false;
  double gt[] = {128.70, .0002, 0., 37.72, 0., -.0002};
  OGRSpatialReference srs;
  if (srs.importFromEPSG(4326) != OGRERR_NONE || dataset->SetSpatialRef(&srs) != CE_None ||
      dataset->SetGeoTransform(gt) != CE_None) return false;
  std::vector<float> z(200 * 160);
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 200; ++x)
      z[static_cast<size_t>(y) * 200 + x] = static_cast<float>(low + (high - low) * x / 199.);
  return dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 200, 160, z.data(), 200, 160,
                                            GDT_Float32, 0, 0) == CE_None;
}
QImage render(const QgsMapSettings& settings, QString* error) {
  QgsMapRendererParallelJob job(settings);
  job.start(); job.waitForFinished();
  for (const auto& item : job.errors()) *error += item.message + QLatin1Char('\n');
  return job.renderedImage();
}
QJsonObject imageMetrics(const QImage& image) {
  QSet<QRgb> colors;
  int dark = 0, samples = 0;
  double luminance = 0.;
  for (int y = image.height() / 4; y < image.height() * 3 / 4; y += 3)
    for (int x = image.width() / 4; x < image.width() * 3 / 4; x += 3) {
      const QColor c = image.pixelColor(x, y);
      colors.insert(c.rgb()); ++samples;
      if (std::max({c.red(), c.green(), c.blue()}) < 60) ++dark;
      luminance += .2126 * c.red() + .7152 * c.green() + .0722 * c.blue();
    }
  return {{QStringLiteral("uniqueColors"), colors.size()}, {QStringLiteral("samples"), samples},
          {QStringLiteral("darkPixels"), dark},
          {QStringLiteral("meanLuma"), samples ? luminance / samples : 0.}};
}
}

class DemRenderingTest : public QObject {
  Q_OBJECT
private slots:
  void elevationMapsAndDeliveredLegend_data() {
    QTest::addColumn<QString>("kind"); QTest::addColumn<QString>("workCrs");
    QTest::addColumn<double>("low"); QTest::addColumn<double>("high");
    QTest::addColumn<QString>("realPath");
    struct Range { const char* name; double low; double high; };
    for (const auto* crs : {"EPSG:5186", "EPSG:5187"}) {
      for (const auto& range : {Range{"coast-0-100", 0, 100}, Range{"hills-100-300", 100, 300},
          Range{"mountains-400-1000", 400, 1000}, Range{"high-1000-2000", 1000, 2000},
          Range{"mixed-0-2000", 0, 2000}}) {
        const QByteArray row = QByteArray(range.name) + '-' + crs;
        QTest::newRow(row.constData()) << QString::fromLatin1(range.name)
            << QString::fromLatin1(crs) << range.low << range.high << QString();
      }
      const QString realDir = qEnvironmentVariable("KA_DEM_QA_REAL_DIR");
      if (!realDir.isEmpty()) {
        for (const auto* name : {"gangneung-daegwallyeong", "hallasan", "gangneung-coast"}) {
          const QString path = QDir(realDir).filePath(QString::fromLatin1(name) + QStringLiteral(".tif"));
          if (!QFile::exists(path)) continue;
          const QByteArray row = QByteArray("real-") + name + '-' + crs;
          QTest::newRow(row.constData()) << QStringLiteral("real-") + QString::fromLatin1(name)
              << QString::fromLatin1(crs) << 0. << 0. << path;
        }
      }
    }
  }
  void elevationMapsAndDeliveredLegend() {
    QFETCH(QString, kind); QFETCH(QString, workCrs); QFETCH(double, low); QFETCH(double, high);
    QFETCH(QString, realPath);
    QTemporaryDir files; QVERIFY(files.isValid());
    const QString path = realPath.isEmpty() ? files.filePath(QStringLiteral("dem.tif")) : realPath;
    if (realPath.isEmpty()) QVERIFY(writeFixture(path, low, high));
    QgsProject project; project.setCrs(QgsCoordinateReferenceSystem(workCrs));
    auto* dem = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
    QVERIFY(dem->isValid()); QVERIFY(project.addMapLayer(dem));
    QVERIFY(LayerOps::applyDemElevationStyle(dem));
    LayerOps::markReferenceLayer(dem);
    const QgsCoordinateTransform transform(dem->crs(), project.crs(), &project);
    const QgsRectangle extent = transform.transformBoundingBox(dem->extent());
    QgsMapSettings settings;
    settings.setDestinationCrs(project.crs()); settings.setTransformContext(project.transformContext());
    settings.setOutputSize(QSize(600, 480)); settings.setOutputDpi(96);
    settings.setExtent(extent); settings.setLayers({dem}); settings.setBackgroundColor(Qt::white);
    QString errors;
    const QImage plain = render(settings, &errors); QVERIFY2(errors.isEmpty(), qPrintable(errors));
    QVERIFY(!plain.isNull());
    QgsRasterLayer* shade = LayerOps::ensureDemRelief(&project, dem);
    QVERIFY2(shade, qPrintable(dem->customProperty(QStringLiteral("ka_hgis/dem_relief_error")).toString()));
    QList<QgsMapLayer*> layers{dem};
    if (shade) layers.prepend(shade);
    settings.setLayers(layers);
    const QImage relief = render(settings, &errors); QVERIFY2(errors.isEmpty(), qPrintable(errors));
    QVERIFY(!relief.isNull());
    auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(dem->renderer());
    QVERIFY(renderer && renderer->shader());
    auto* ramp = dynamic_cast<QgsColorRampShader*>(renderer->shader()->rasterShaderFunction());
    QVERIFY(ramp);
    auto* treeNode = project.layerTreeRoot()->findLayer(dem->id()); QVERIFY(treeNode);
    const auto legendNodes = dem->legend()->createLayerTreeModelLegendNodes(treeNode);
    QJsonArray labels;
    for (auto* node : legendNodes) labels.append(node->data(Qt::DisplayRole).toString());
    const int legendCount = legendNodes.size(); qDeleteAll(legendNodes);

    QgsPrintLayout layout(&project); layout.initializeDefaults();
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Landscape);
    auto* map = new QgsLayoutItemMap(&layout);
    map->attemptSetSceneRect(QRectF(10, 15, 200, 160));
    map->setCrs(project.crs()); map->setKeepLayerSet(true); map->setLayers(layers);
    map->zoomToExtent(extent); layout.addLayoutItem(map);
    auto* legend = new QgsLayoutItemLegend(&layout);
    legend->setTitle(QStringLiteral("표고 (m)")); legend->setLinkedMap(map);
    legend->attemptSetSceneRect(QRectF(219, 15, 63, 145)); layout.addLayoutItem(legend);
    LayoutService::tuneSheetLegend(legend);
    QgsLayoutExporter exporter(&layout);
    const QImage preview = exporter.renderPageToImage(0, QSize(), 120.);
    QVERIFY(!preview.isNull());
    const bool before = qEnvironmentVariable("KA_DEM_QA_PHASE") == QLatin1String("before");
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      const QString stem = QDir(output).filePath(kind + QLatin1Char('-') + workCrs.mid(5));
      QVERIFY(plain.save(stem + QStringLiteral("-map.png")));
      QVERIFY(relief.save(stem + QStringLiteral("-relief.png")));
      QVERIFY(preview.save(stem + QStringLiteral("-preview.png")));
      QgsLayoutExporter::PdfExportSettings pdf; pdf.dpi = 120.;
      QCOMPARE(exporter.exportToPdf(stem + QStringLiteral(".pdf"), pdf), QgsLayoutExporter::Success);
      QgsLayerTreeModel model(project.layerTreeRoot());
      QgsLayerTreeView tree; tree.setModel(&model); tree.resize(260, 500); tree.expandAll();
      tree.show(); QCoreApplication::processEvents();
      QVERIFY(tree.grab().save(stem + QStringLiteral("-tree.png")));
      QJsonObject metadata{{QStringLiteral("kind"), kind}, {QStringLiteral("workCrs"), workCrs},
          {QStringLiteral("sourceKind"), realPath.isEmpty() ? QStringLiteral("synthetic elevation ramp") : QStringLiteral("public Copernicus GLO-30 subset")},
          {QStringLiteral("sourceCrs"), dem->crs().authid()}, {QStringLiteral("mapScale"), settings.scale()},
          {QStringLiteral("extent"), extent.toString()}, {QStringLiteral("plain"), imageMetrics(plain)},
          {QStringLiteral("relief"), imageMetrics(relief)}, {QStringLiteral("legendNodes"), legendCount},
          {QStringLiteral("legendLabels"), labels}, {QStringLiteral("layoutLegend"), LayoutService::sheetLegendLabelDump(legend)},
          {QStringLiteral("legendWidthMm"), legend->sizeWithUnits().width()},
          {QStringLiteral("legendHeightMm"), legend->sizeWithUnits().height()},
          {QStringLiteral("classificationMin"), renderer->classificationMin()},
          {QStringLiteral("classificationMax"), renderer->classificationMax()}};
      QFile json(stem + QStringLiteral(".json")); QVERIFY(json.open(QIODevice::WriteOnly));
      QVERIFY(json.write(QJsonDocument(metadata).toJson()) > 0);
    }
    if (!before) {
      QCOMPARE(ramp->colorRampType(), Qgis::ShaderInterpolationMethod::Linear);
      QVERIFY2(legendCount <= 2, "One compact elevation ramp must replace many legend class rows");
      if (realPath.isEmpty()) {
        const auto metrics = imageMetrics(plain);
        QVERIFY2(metrics.value(QStringLiteral("uniqueColors")).toInt() > 20,
                 "A smooth terrain slope must have visibly graduated elevation colors");
        QCOMPARE(metrics.value(QStringLiteral("darkPixels")).toInt(), 0);
        QVERIFY2(imageMetrics(relief).value(QStringLiteral("meanLuma")).toDouble() > 60.,
                 "Geographic DEM shading must not turn the map almost black");
      }
    }
  }
};
int main(int argc, char** argv) {
  CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
  QgsApplication app(argc, argv, true);
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgun.ttf"));
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgunbd.ttf"));
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  DemRenderingTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis(); return result;
}
#include "test_dem_rendering.moc"

