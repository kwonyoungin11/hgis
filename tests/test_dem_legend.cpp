#include <QtTest>
#include <QTemporaryDir>
#include <QPainter>
#include <QPointer>
#include <QFontDatabase>
#include <QDir>
#include <gdal_priv.h>
#include <qgsapplication.h>
#include <qgscolorramplegendnode.h>
#include <qgscolorrampshader.h>
#include <qgslayertreelayer.h>
#include <qgslegendsettings.h>
#include <qgscolorramp.h>
#include <qgsrasterlayer.h>
#include <qgsrasterdataprovider.h>
#include <qgsrastershader.h>
#include <qgsrendercontext.h>
#include <qgsrasterblock.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include "core/DemColorRampLegend.h"
#include "core/DemPresentation.h"

class DemLegendTest : public QObject {
  Q_OBJECT
  QTemporaryDir m_files;
  std::unique_ptr<QgsRasterLayer> raster() {
    GDALAllRegister();
    const QString path = m_files.filePath(QStringLiteral("dem.tif"));
    auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) return {};
    auto* dataset = driver->Create(path.toUtf8().constData(), 2, 2, 1, GDT_Float32, nullptr);
    if (!dataset) return {};
    double transform[] = {190000., 1., 0., 560000., 0., -1.};
    dataset->SetGeoTransform(transform);
    float elevations[] = {0.f, 200.f, 1000.f, 2000.f};
    const CPLErr wrote = dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 2, 2, elevations, 2, 2,
                                                          GDT_Float32, 0, 0, nullptr);
    GDALClose(dataset);
    if (wrote != CE_None) return {};
    return std::make_unique<QgsRasterLayer>(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  }
  static void colorize(QgsRasterLayer* layer, double maximum = 2000.) {
    auto* ramp = new QgsColorRampShader(0., maximum);
    ramp->setColorRampType(Qgis::ShaderInterpolationMethod::Linear);
    ramp->setColorRampItemList({{0., QColor(30, 130, 50)},
                              {maximum / 2., QColor(210, 170, 70)},
                              {maximum, QColor(160, 70, 30)}});
    auto* shader = new QgsRasterShader(0., maximum);
    shader->setRasterShaderFunction(ramp);
    auto* renderer = new QgsSingleBandPseudoColorRenderer(layer->dataProvider(), 1, shader);
    renderer->setClassificationMin(0.);
    renderer->setClassificationMax(maximum);
    layer->setRenderer(renderer);
  }
private slots:
  void installedLegendHasOneContinuousNode() {
    auto layer = raster(); QVERIFY(layer && layer->isValid());
    QVERIFY(DemPresentation::apply(layer.get()));
    QVERIFY(DemColorRampLegend::install(layer.get()));
    QgsLayerTreeLayer tree(layer.get());
    const auto nodes = layer->legend()->createLayerTreeModelLegendNodes(&tree);
    QCOMPARE(nodes.size(), 1);
    auto* ramp = dynamic_cast<QgsColorRampLegendNode*>(nodes.first());
    QVERIFY(ramp);
    QCOMPARE(ramp->minimum(), 0.);
    QCOMPARE(ramp->maximum(), 2000.);
    const QString description = ramp->data(Qt::ToolTipRole).toString();
    QVERIFY(description.contains(QStringLiteral("표고 (m)")));
    for (const auto tick : {"0", "100", "200", "500", "1000", "1500", "2000"})
      QVERIFY(description.contains(QString::fromLatin1(tick)));
    QVERIFY(!description.contains(QStringLiteral("Band")));
    const QPixmap preview = ramp->data(Qt::DecorationRole).value<QPixmap>();
    QVERIFY(!preview.isNull());
    const qreal logicalHeight = preview.height() / preview.devicePixelRatio();
    QVERIFY(logicalHeight >= 130. && logicalHeight <= 160.);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QVERIFY(preview.save(QDir(output).filePath(QStringLiteral("dem-legend-tree.png"))));
    }
    // At the seven equal-spaced ticks, the adapter must use the renderer's
    // elevation colors, including the lowland stops at 100 and 200 m.
    const auto* renderer = dynamic_cast<const QgsSingleBandPseudoColorRenderer*>(layer->renderer());
    const auto* shader = dynamic_cast<const QgsColorRampShader*>(renderer->shader()->rasterShaderFunction());
    const double values[] = {0., 100., 200., 500., 1000., 1500., 2000.};
    for (int i = 0; i < 7; ++i) {
      int r, g, b, a;
      QVERIFY(shader->shade(values[i], &r, &g, &b, &a));
      QCOMPARE(ramp->ramp()->color(i / 6.), QColor(r, g, b, a));
    }
    qDeleteAll(nodes);
  }
  void layoutAndTreeUseSameColorSamples() {
    auto layer = raster(); QVERIFY(layer && layer->isValid());
    QVERIFY(DemPresentation::apply(layer.get()));
    QgsLayerTreeLayer tree(layer.get());
    const auto nodes = layer->legend()->createLayerTreeModelLegendNodes(&tree);
    QCOMPARE(nodes.size(), 1);
    auto* node = nodes.first();
    QImage preview = node->data(Qt::DecorationRole).value<QPixmap>().toImage();
    QgsLegendSettings settings;
    const QSizeF mm = node->drawSymbol(settings, nullptr, 0.);
    QVERIFY(mm.width() > 0. && mm.height() >= 32. && mm.height() <= 42.);
    QImage output(preview.size(), QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    painter.scale(output.width() / mm.width(), output.height() / mm.height());
    QgsLayerTreeModelLegendNode::ItemContext context;
    context.painter = &painter;
    context.columnRight = mm.width();
    node->drawSymbol(settings, &context, 0.);
    painter.end();
    const QString outputDir = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!outputDir.isEmpty()) {
      QVERIFY(QDir().mkpath(outputDir));
      QVERIFY(output.save(QDir(outputDir).filePath(QStringLiteral("dem-legend-layout-symbol.png"))));
    }
    // Compare four actual raster-rendered cells to shader and legend pixels,
    // including lowland 200 m and both range endpoints.
    auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(layer->renderer());
    const auto* shader = dynamic_cast<const QgsColorRampShader*>(renderer->shader()->rasterShaderFunction());
    std::unique_ptr<QgsRasterBlock> block(renderer->block(1, layer->extent(), 2, 2));
    QVERIFY(block && block->isValid());
    const double elevations[] = {0., 200., 1000., 2000.};
    for (int index = 0; index < 4; ++index) {
      int r, g, b, a;
      QVERIFY(shader->shade(elevations[index], &r, &g, &b, &a));
      const QColor expected(r, g, b, a);
      QCOMPARE(QColor::fromRgba(block->color(index / 2, index % 2)), expected);
      qInfo().noquote() << QStringLiteral("Rendered elevation %1 m: RGB %2,%3,%4")
          .arg(elevations[index]).arg(r).arg(g).arg(b);
      const auto contains = [expected](const QImage& image) {
        for (int y = 0; y < image.height(); ++y)
          for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (qAbs(color.red() - expected.red()) <= 2 && qAbs(color.green() - expected.green()) <= 2 &&
                qAbs(color.blue() - expected.blue()) <= 2 && color.alpha() > 250) return true;
          }
        return false;
      };
      QVERIFY2(contains(preview), qPrintable(QStringLiteral("Tree legend omits %1 m (%2)").arg(elevations[index]).arg(expected.name())));
      QVERIFY2(contains(output), qPrintable(QStringLiteral("Layout legend omits %1 m (%2)").arg(elevations[index]).arg(expected.name())));
    }
    qDeleteAll(nodes);
  }
  void grayRendererRemainsUnchanged() {
    auto layer = raster(); QVERIFY(layer && layer->isValid());
    auto* legend = layer->legend();
    QVERIFY(!DemColorRampLegend::install(layer.get()));
    QCOMPARE(layer->legend(), legend);
  }
  void styleChangesAndLayerDestructionAreSafe() {
    auto layer = raster(); QVERIFY(layer && layer->isValid());
    colorize(layer.get()); QVERIFY(DemColorRampLegend::install(layer.get()));
    QPointer<QgsMapLayerLegend> legend(layer->legend());
    QSignalSpy changed(legend, &QgsMapLayerLegend::itemsChanged);
    colorize(layer.get(), 80.);
    layer->triggerRepaint();
    QVERIFY(changed.count() > 0);
    QgsLayerTreeLayer tree(layer.get());
    const auto nodes = legend->createLayerTreeModelLegendNodes(&tree);
    QCOMPARE(nodes.size(), 1);
    auto* ramp = dynamic_cast<QgsColorRampLegendNode*>(nodes.first());
    QVERIFY(ramp); QCOMPARE(ramp->maximum(), 80.);
    layer.reset();
    QVERIFY(legend.isNull());
    QVERIFY(!ramp->data(Qt::DecorationRole).value<QPixmap>().isNull());
    qDeleteAll(nodes);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString fonts = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts/");
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    if (QFontDatabase::addApplicationFont(fonts + file) < 0) return 2;
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  DemLegendTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_dem_legend.moc"
