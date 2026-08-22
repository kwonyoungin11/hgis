// Task 1 (RED): SectionLayoutService ?? ?? ?? ?? ???
// Task 2 (GREEN): SectionLayoutService::axisTicks / niceDistanceInterval ?? ? ??
// Task 3 (RED->GREEN): QGIS ?? ?? ?? ???

#include <cmath>
#include <limits>
#include <vector>

#include <QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVector>

#include "core/SectionLayoutService.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitempolyline.h>
#include <qgslinesymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutitempage.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

#include <gdal.h>
#include <cpl_conv.h>
#include <ogr_spatialref.h>

class TestSectionLayout : public QObject {
    Q_OBJECT
private slots:
    // ---- ?? ?? ----
    void initTestCase();
    void cleanupTestCase();

    // ---- Task 1: axisTicks ?? ----
    void elevationTicksNominal();      // 100.01~100.41, 0.10 -> 4?
    void relativeDistanceTicks();      // 0.0~2.0, 0.5 -> 5?
    void exactBoundaryIncluded();      // 0.0~1.0, 0.5 -> [0.0, 0.5, 1.0] ?? ??

    // ---- Task 1: ?? 1-2-5 ?? ?? ----
    void niceIntervalFor2m();          // span=2.0 -> 0.5
    void niceIntervalFor10m();         // span=10.0 -> 2.0
    void niceIntervalFor100m();        // span=100.0 -> 20.0

    // ---- Task 1: ?? ?? ----
    void invalidIntervalZero();        // interval=0 -> ??
    void invalidIntervalNegative();    // interval=-0.1 -> ??
    void invalidRangeNonFiniteMax();   // maxVal=inf -> ??
    void invalidRangeNonFiniteMin();   // minVal=nan -> ??
    void invalidRangeReversed();       // min>max -> ??
    void invalidRangeEqual();          // min==max -> ??
    void maxTicksExceeded();           // 0.0~1000.0, 0.1 -> 500? ?? ??

    // ---- Task 2: ?? ??? ??? ----
    void integerIndexPrecision();      // ????? ?? ?? ??? ?

    // ---- ?? ?? P2/P3 ?? ----
    void niceIntervalNonFiniteSpan();  // NaN/Inf span -> 1.0 (P2 API ??)
    void axisTicksFirstIdxOverflow();  // ??? long long ?? ?? -> ??? ?? (P3 UB)

    // ---- Task 3: QGIS ???? ?? ??? ----
    void buildLayoutA3();              // A3 ?? 420x297, map CRS=EPSG:5186
    void buildLayoutA4();              // A4 ?? 297x210
    void buildEmptyPaperHasGrid();     // empty layers -> paper + ticks
    void buildLayoutUserCrsLabel();    // mapCrsAuthId overrides raster CRS label
    void buildLayoutUnrotatesWorldGeoref(); // rotated map GT -> 10x2 section plane
    void buildLayoutWorldPlacementSitsHorizontal(); // ??-like map XY, Y not northing
    void buildLayoutKeepsOrthometricElev(); // shear + ?? 100..102 must stay 2.00m
    void buildLayoutElevAxisExists();  // ka_section_elevation_0 ??? ??
    void buildLayoutDistAxisStartsZero(); // ka_section_distance_0 ?? "0.00m"
    void buildLayoutReferenceLineStyle(); // ??? #D7191C, ??, 0.20mm
    void buildLayoutAppliesCustomLineStyle(); // 옵션 굵기·색이 기준선에 적용
    void buildLayoutHasScaleBarSamples(); // Double Box + 샘플 3종
    void buildLayoutChromeExists();    // ???.??.??? ?? ??
    void exportSectionPdfNotEmpty();   // PDF ?? ?? & ?? > 100 ???
    // ---- Task 3 ???: raster ? ??? ? map item ??? < 0.5mm ----
    void buildLayoutRasterCornersAligned();
    // ---- Task 3 ???: scaleDenominator ?? ?? ----
    void buildLayoutScaleDenominator();

private:
    QString m_tiffPath; // Task 3 ???? ?? GeoTIFF ??
};

// ---- ?? ----
static void checkTicks(const AxisTickResult &r,
                       const QVector<double> &expected,
                       double tol = 1e-9)
{
    QVERIFY2(r.error.isEmpty(),
             qPrintable(QStringLiteral("?? ??: ") + r.error));
    QCOMPARE(r.ticks.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        const double diff = std::abs(r.ticks[i] - expected[i]);
        QVERIFY2(diff < tol,
                 qPrintable(QStringLiteral("ticks[%1]: got %2, expected %3, diff %4")
                                .arg(i)
                                .arg(r.ticks[i], 0, 'g', 15)
                                .arg(expected[i], 0, 'g', 15)
                                .arg(diff, 0, 'e', 3)));
    }
}

// ---- ?? ??: 100.01~100.41, ?? 0.10 ----
void TestSectionLayout::elevationTicksNominal()
{
    const auto r = SectionLayoutService::axisTicks(100.01, 100.41, 0.10);
    checkTicks(r, {100.10, 100.20, 100.30, 100.40});
}

// ---- ?? ?? ??: 0.0~2.0m, ?? 0.5 ----
void TestSectionLayout::relativeDistanceTicks()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 2.0, 0.5);
    checkTicks(r, {0.0, 0.5, 1.0, 1.5, 2.0});
}

// ---- ??? ?? ----
void TestSectionLayout::exactBoundaryIncluded()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, 0.5);
    checkTicks(r, {0.0, 0.5, 1.0});
}

// ---- ?? 1-2-5 ?? ?? ----
void TestSectionLayout::niceIntervalFor2m()
{
    const double v = SectionLayoutService::niceDistanceInterval(2.0);
    QVERIFY2(std::abs(v - 0.5) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(2.0)=%1, expected 0.5").arg(v)));
}

void TestSectionLayout::niceIntervalFor10m()
{
    const double v = SectionLayoutService::niceDistanceInterval(10.0);
    QVERIFY2(std::abs(v - 2.0) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(10.0)=%1, expected 2.0").arg(v)));
}

void TestSectionLayout::niceIntervalFor100m()
{
    const double v = SectionLayoutService::niceDistanceInterval(100.0);
    QVERIFY2(std::abs(v - 20.0) < 1e-12,
             qPrintable(QStringLiteral("niceDistanceInterval(100.0)=%1, expected 20.0").arg(v)));
}

// ---- ?? ?? ----
void TestSectionLayout::invalidIntervalZero()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, 0.0);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidIntervalNegative()
{
    const auto r = SectionLayoutService::axisTicks(0.0, 1.0, -0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeNonFiniteMax()
{
    const auto r = SectionLayoutService::axisTicks(0.0,
                                                   std::numeric_limits<double>::infinity(),
                                                   0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeNonFiniteMin()
{
    const auto r = SectionLayoutService::axisTicks(std::numeric_limits<double>::quiet_NaN(),
                                                   1.0,
                                                   0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeReversed()
{
    const auto r = SectionLayoutService::axisTicks(5.0, 3.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::invalidRangeEqual()
{
    const auto r = SectionLayoutService::axisTicks(2.0, 2.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

void TestSectionLayout::maxTicksExceeded()
{
    // 0.0~1000.0, ?? 0.1 -> 10001? ??, 500 ?? ??
    const auto r = SectionLayoutService::axisTicks(0.0, 1000.0, 0.1);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
}

// ---- ?? ??? ??? (????? ?? ??) ----
void TestSectionLayout::integerIndexPrecision()
{
    // 0.0~0.50, ?? 0.10 -> 6?: 0.00, 0.10, 0.20, 0.30, 0.40, 0.50
    const auto r = SectionLayoutService::axisTicks(0.0, 0.50, 0.10);
    checkTicks(r, {0.00, 0.10, 0.20, 0.30, 0.40, 0.50});
}

// ---- ?? ?? P2: niceDistanceInterval NaN/Inf span -> 1.0 ----
void TestSectionLayout::niceIntervalNonFiniteSpan()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    QCOMPARE(SectionLayoutService::niceDistanceInterval(nan), 1.0);
    QCOMPARE(SectionLayoutService::niceDistanceInterval(inf), 1.0);
    QCOMPARE(SectionLayoutService::niceDistanceInterval(-inf), 1.0);
}

// ---- ?? ?? P3: axisTicks firstIdx long long ???? -> ??? ?? ----
void TestSectionLayout::axisTicksFirstIdxOverflow()
{
    // 1.0e19 > LLONG_MAX ~= 9.22e18 ??? rawIdx = ceil(1e19/1.0) = 1e19 > kIdxMax
    const auto r = SectionLayoutService::axisTicks(1.0e19, 2.0e19, 1.0);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.ticks.isEmpty());
    // "???" in UTF-8: EC 9D B8 EB 8D B1 EC 8A A4
    QVERIFY2(r.error.contains(QString::fromUtf8("\xec\x9d\xb8\xeb\x8d\xb1\xec\x8a\xa4")),
             qPrintable(QStringLiteral("error missing index keyword (P3 guard missing): ") + r.error));
}

// =========================================================
// Task 3: QGIS ?? ?? ???
// =========================================================

// ---- ???: ?? GeoTIFF ?? ----
// X=0..10m (??), Y=100..102m (??), EPSG:5186, 10x20 ??, Float32
void TestSectionLayout::initTestCase()
{
    GDALAllRegister();
    m_tiffPath = QDir::temp().filePath(QStringLiteral("ka_test_section_task3.tif"));

    GDALDriverH drv = GDALGetDriverByName("GTiff");
    if (!drv) { qWarning("GTiff ???? ?? - Task 3 ??? ???"); return; }

    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(m_tiffPath), 10, 20, 1, GDT_Float32, nullptr);
    if (!ds) { m_tiffPath.clear(); qWarning("GeoTIFF ?? ??"); return; }

    // geotransform: ??(0, 102), ?? 1m?(-0.1m) -> X=0..10, Y=100..102
    double gt[6] = {0.0, 1.0, 0.0, 102.0, 0.0, -0.1};
    GDALSetGeoTransform(ds, gt);

    // CRS: EPSG:5186
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);

    // ???: ?? ??? 101.0?? ??
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20,
                 data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    if (!QFile::exists(m_tiffPath))
        m_tiffPath.clear();
}

void TestSectionLayout::cleanupTestCase()
{
    if (!m_tiffPath.isEmpty())
        QFile::remove(m_tiffPath);
}

// ---- ??: ???? ?? + ???? ??? ??? ?? ----
// QgsProject? raster layer? ????? ???? layer? delete ?? ???.
static SectionLayoutResult buildTestLayout(
    QgsProject* proj,
    const QString& tiffPath,
    QgsRasterLayer** outLayer,
    SectionLayoutOptions::Paper paper = SectionLayoutOptions::Paper::A3)
{
    proj->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsRasterLayer(tiffPath,
                                     QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    if (!layer->isValid()) {
        delete layer; // ???? ??? ???? ?? ? ?? ??
        if (outLayer) *outLayer = nullptr;
        return SectionLayoutResult{QString{}, 0.0, QgsRectangle{},
                                   QStringLiteral("??? ?? ? GeoTIFF ??")};
    }
    // ????? ???? ??? proj ?? ? layer? ?? ????.
    proj->addMapLayer(layer, /*addToLegend=*/false);
    if (outLayer) *outLayer = layer;

    SectionLayoutOptions opts;
    opts.paper = paper;
    return SectionLayoutService::buildSectionLayout(proj, {layer}, opts);
}

// ---- A3 ??: ??? 420x297, map CRS = EPSG:5186 ----
void TestSectionLayout::buildLayoutA3()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer,
                                     SectionLayoutOptions::Paper::A3);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ?? ??");

    // ??? ??: A3 ?? 420x297
    QVERIFY(ly->pageCollection() && ly->pageCollection()->pageCount() > 0);
    const QgsLayoutSize ps = ly->pageCollection()->page(0)->pageSize();
    QVERIFY2(std::abs(ps.width()  - 420.0) < 1.0 &&
             std::abs(ps.height() - 297.0) < 1.0,
             qPrintable(QStringLiteral("Page %1x%2 != 420x297").arg(ps.width()).arg(ps.height())));

    // ?? ?? ??
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");

    // ?? CRS = GeoTIFF EPSG:5186 (???? EPSG:5187 ??)
    QCOMPARE(mapItem->crs().authid(), QStringLiteral("EPSG:5186"));

    // ?? ??? ?? ?? ??
    const QRectF mapRect = mapItem->sceneBoundingRect();
    QVERIFY2(mapRect.left()   >= -0.5 && mapRect.right()  <= 421.0 &&
             mapRect.top()    >= -0.5 && mapRect.bottom() <= 298.0,
             qPrintable(QStringLiteral("map rect ?? ??: %1,%2,%3,%4")
                            .arg(mapRect.left()).arg(mapRect.top())
                            .arg(mapRect.right()).arg(mapRect.bottom())));
}

// ---- A4 ??: ??? 297x210 ----
void TestSectionLayout::buildLayoutA4()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer,
                                     SectionLayoutOptions::Paper::A4);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ?? ??");

    const QgsLayoutSize ps = ly->pageCollection()->page(0)->pageSize();
    QVERIFY2(std::abs(ps.width()  - 297.0) < 1.0 &&
             std::abs(ps.height() - 210.0) < 1.0,
             qPrintable(QStringLiteral("Page %1x%2 != 297x210").arg(ps.width()).arg(ps.height())));
}

void TestSectionLayout::buildEmptyPaperHasGrid()
{
    QgsProject proj;
    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(res.layoutName == QStringLiteral("section_sheet"),
             "empty paper should create section_sheet");

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_0")),
             "empty paper needs elevation ticks");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_distance_0")),
             "empty paper needs distance ticks");
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5187")),
             qPrintable(QStringLiteral("CRS label: ") + crsLbl->text()));
}

void TestSectionLayout::buildLayoutUserCrsLabel()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    auto* added = new QgsRasterLayer(m_tiffPath, QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    QVERIFY2(added->isValid(), "test GeoTIFF invalid");
    proj.addMapLayer(added, false);
    layer = added;
    Q_UNUSED(layer);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {added}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5187")),
             qPrintable(QStringLiteral("user CRS should label 5187, got ") + crsLbl->text()));
}

void TestSectionLayout::buildLayoutUnrotatesWorldGeoref()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_rotated.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Float32, nullptr);
    QVERIFY2(ds, "rotated GeoTIFF create failed");

    const double c = 0.7071067811865476;
    double gt[6] = {
        200000.0,
        1.0 * c,
        0.1 * c,
        450000.0,
        1.0 * c,
        -0.1 * c
    };
    GDALSetGeoTransform(ds, gt);
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("rotated"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "rotated layer invalid");
    QVERIFY2(layer->extent().height() > 4.0, "world AABB should be taller than 2m");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5187");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.height() - 2.0) < 0.20,
             qPrintable(QStringLiteral("height %1 expected ~2m")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.width() - 10.0) < 0.20,
             qPrintable(QStringLiteral("width %1 expected ~10m")
                            .arg(res.appliedExtent.width(), 0, 'f', 3)));
    QFile::remove(path);
}

void TestSectionLayout::buildLayoutWorldPlacementSitsHorizontal()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_gangneung_like.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Byte, nullptr);
    QVERIFY2(ds, "world-placement GeoTIFF create failed");

    // ??.tif? ?? ??: ?? ~1m, ??, ??? ?? XY. Z ??.
    double gt[6] = {
        194884.7739783798,
        0.8684753187746654,
        0.4957324083476341,
        574016.921990569,
        0.4957324083499024,
        -0.868475318770692
    };
    GDALSetGeoTransform(ds, gt);
    std::vector<unsigned char> data(10 * 20, 180);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Byte, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("gangneung_like"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "world-placement layer invalid");
    QVERIFY2(layer->extent().yMinimum() > 10000.0, "source Y should be map northing");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.mapCrsAuthId = QStringLiteral("EPSG:5186");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.xMinimum()) < 0.05,
             qPrintable(QStringLiteral("xMin %1 should be ~0 (distance)")
                            .arg(res.appliedExtent.xMinimum(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.yMinimum()) < 0.05,
             qPrintable(QStringLiteral("yMin %1 should be ~0, not northing")
                            .arg(res.appliedExtent.yMinimum(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.width() - 10.0) < 0.20,
             qPrintable(QStringLiteral("width %1 expected ~10m")
                            .arg(res.appliedExtent.width(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.height() - 20.0) < 0.20,
             qPrintable(QStringLiteral("height %1 expected ~20m (photo Y)")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY(mapItem);
    QVERIFY2(std::abs(mapItem->mapRotation()) < 1e-9, "map must stay unrotated");
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5186")),
             qPrintable(QStringLiteral("title CRS: ") + crsLbl->text()));
    QFile::remove(path);
}

void TestSectionLayout::buildLayoutKeepsOrthometricElev()
{
    const QString path = QDir::temp().filePath(QStringLiteral("ka_test_section_elev.tif"));
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    QVERIFY2(drv, "GTiff driver missing");
    GDALDatasetH ds = GDALCreate(drv, qUtf8Printable(path), 10, 20, 1, GDT_Float32, nullptr);
    QVERIFY2(ds, "elev GeoTIFF create failed");

    // ?? ?? + ?? 102?100 (gt[5]=-0.1). hypot(gt[2],gt[5])? ??? 2m? ??.
    double gt[6] = {0.0, 1.0, 0.05, 102.0, 0.0, -0.1};
    GDALSetGeoTransform(ds, gt);
    OGRSpatialReferenceH srs = OSRNewSpatialReference(nullptr);
    OSRImportFromEPSG(srs, 5186);
    char* wkt = nullptr;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);
    std::vector<float> data(10 * 20, 101.0f);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(band, GF_Write, 0, 0, 10, 20, data.data(), 10, 20, GDT_Float32, 0, 0);
    GDALClose(ds);

    QgsProject proj;
    auto* layer = new QgsRasterLayer(path, QStringLiteral("elev"), QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "elev layer invalid");
    proj.addMapLayer(layer, false);

    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, SectionLayoutOptions{});
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));
    QVERIFY2(std::abs(res.appliedExtent.height() - 2.0) < 0.05,
             qPrintable(QStringLiteral("?? ?? %1m, ?? 2.00m")
                            .arg(res.appliedExtent.height(), 0, 'f', 3)));
    QVERIFY2(std::abs(res.appliedExtent.yMinimum() - 100.0) < 0.05,
             qPrintable(QStringLiteral("?? ?? %1, ?? 100")
                            .arg(res.appliedExtent.yMinimum(), 0, 'f', 3)));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_elevation_0")));
    QVERIFY(lbl0);
    QCOMPARE(lbl0->text(), QStringLiteral("100.00"));
    QFile::remove(path);
}

// ---- ?? ? ??? ?? ?? ----
// GeoTIFF Y=100..102, interval=0.10 -> ka_section_elevation_0 = "100.00"
void TestSectionLayout::buildLayoutElevAxisExists()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    // ? ?? ??? ?? ? ??? ??
    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_elevation_0")));
    QVERIFY2(lbl0, "ka_section_elevation_0 ??");
    QCOMPARE(lbl0->text(), QStringLiteral("100.00"));

    // ?? tick ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_tick_0")),
             "ka_section_elevation_tick_0 ??");

    // ?? ?? ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_axis")),
             "ka_section_elevation_axis ??");

    auto* grid0 = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_elevation_grid_0")));
    QVERIFY2(grid0, "ka_section_elevation_grid_0 missing (0.10m 해발 가로선)");
    const QPolygonF gridPts = grid0->nodes();
    QVERIFY2(gridPts.size() >= 2, "elevation grid must span the map");
    QVERIFY2(std::abs(gridPts.first().x() - gridPts.last().x()) > 50.0,
             "elevation grid must be a full-width horizontal line");
}

// ---- ?? ? ? ??? = "0.00m" ----
void TestSectionLayout::buildLayoutDistAxisStartsZero()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    auto* lbl0 = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_distance_0")));
    QVERIFY2(lbl0, "ka_section_distance_0 ??");
    QCOMPARE(lbl0->text(), QStringLiteral("0.00m"));

    // ?? ?? ?? ??
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_distance_axis")),
             "ka_section_distance_axis ??");
}

// ---- ???: #D7191C, ??, ??? 0.20mm ----
void TestSectionLayout::buildLayoutReferenceLineStyle()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    auto* refItem = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY2(refItem, "ka_section_reference_line ??");

    QgsLineSymbol* sym = refItem->symbol();
    QVERIFY2(sym && sym->symbolLayerCount() > 0, "??? ?? ??");

    auto* sl = dynamic_cast<QgsSimpleLineSymbolLayer*>(sym->symbolLayer(0));
    QVERIFY2(sl, "?? ???? QgsSimpleLineSymbolLayer ??");

    // ?? #D7191C = RGB(215, 25, 28)
    const QColor expected(0xD7, 0x19, 0x1C);
    QCOMPARE(sl->color().rgb(), expected.rgb());

    // ?? 0.20mm
    QVERIFY2(std::abs(sl->width() - 0.20) < 0.01,
             qPrintable(QStringLiteral("??? ?? %1mm, ?? 0.20mm").arg(sl->width())));

    // ?? ??
    QCOMPARE(sl->penStyle(), Qt::DashLine);
}

void TestSectionLayout::buildLayoutAppliesCustomLineStyle()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    auto* layer = new QgsRasterLayer(m_tiffPath, QStringLiteral("test_section"),
                                     QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "test GeoTIFF invalid");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.referenceLineWidthMm = 0.45;
    opts.referenceLineColor = QStringLiteral("#0B3D91");
    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);
    auto* refItem = qobject_cast<QgsLayoutItemPolyline*>(
        ly->itemById(QStringLiteral("ka_section_reference_line")));
    QVERIFY2(refItem, "ka_section_reference_line missing");
    auto* sl = dynamic_cast<QgsSimpleLineSymbolLayer*>(refItem->symbol()->symbolLayer(0));
    QVERIFY(sl);
    QCOMPARE(sl->color().rgb(), QColor(QStringLiteral("#0B3D91")).rgb());
    QVERIFY2(std::abs(sl->width() - 0.45) < 0.01,
             qPrintable(QStringLiteral("width %1mm, expected 0.45mm").arg(sl->width())));
}

void TestSectionLayout::buildLayoutHasScaleBarSamples()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF missing");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar")),
             "ka_section_scale_bar missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar_single")),
             "ka_section_scale_bar_single missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar_ticks")),
             "ka_section_scale_bar_ticks missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar_numeric")),
             "ka_section_scale_bar_numeric missing");

    auto* single = qobject_cast<QgsLayoutItemScaleBar*>(
        ly->itemById(QStringLiteral("ka_section_scale_bar_single")));
    QVERIFY(single);
    QCOMPARE(single->style(), QStringLiteral("Single Box"));

    auto* ticks = qobject_cast<QgsLayoutItemScaleBar*>(
        ly->itemById(QStringLiteral("ka_section_scale_bar_ticks")));
    QVERIFY(ticks);
    QCOMPARE(ticks->style(), QStringLiteral("Line Ticks Up"));

    auto* numeric = qobject_cast<QgsLayoutItemScaleBar*>(
        ly->itemById(QStringLiteral("ka_section_scale_bar_numeric")));
    QVERIFY(numeric);
    QCOMPARE(numeric->style(), QStringLiteral("Numeric"));
}

// ---- ???.??.??? ?? ?? ----
void TestSectionLayout::buildLayoutChromeExists()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY(ly);

    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale_bar")),
             "ka_section_scale_bar ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_title_block")),
             "ka_section_title_block ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_crs")),
             "ka_section_crs ??");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_scale")),
             "ka_section_scale ??");

    // ??? ?? ?? ?? ??
    auto* tb = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_title_block")));
    QVERIFY(tb);
    QVERIFY2(tb->text().contains(QStringLiteral("5186")) ||
             tb->text().contains(QStringLiteral("m")) ,
             qPrintable(QStringLiteral("?? ??? ??: ") + tb->text()));

    // ??? ???? "EPSG:5186" ?? ??
    auto* crsLbl = qobject_cast<QgsLayoutItemLabel*>(
        ly->itemById(QStringLiteral("ka_section_crs")));
    QVERIFY(crsLbl);
    QVERIFY2(crsLbl->text().contains(QStringLiteral("5186")),
             qPrintable(QStringLiteral("CRS ???? '5186' ??: ") + crsLbl->text()));
}

// ---- PDF ????: ?? ?? & ?? > 100 ??? ----
void TestSectionLayout::exportSectionPdfNotEmpty()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    QgsRasterLayer* layer = nullptr;
    const auto res = buildTestLayout(&proj, m_tiffPath, &layer);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    const QString pdfPath = QDir::temp().filePath(
        QStringLiteral("ka_test_section_layout_task3.pdf"));
    QFile::remove(pdfPath);

    QString errStr;
    const QString out = SectionLayoutService::exportSectionPdf(&proj, pdfPath, &errStr);
    QVERIFY2(!out.isEmpty(),
             qPrintable(QStringLiteral("exportSectionPdf ??: ") + errStr));
    QVERIFY2(QFile::exists(pdfPath), "PDF ?? ???? ??");
    QVERIFY2(QFileInfo(pdfPath).size() > 100,
             qPrintable(QStringLiteral("PDF ?? %1 ??? (?? ??)")
                            .arg(QFileInfo(pdfPath).size())));

    QFile::remove(pdfPath);
}

// ---- [RED] raster ? ??? ? map item ? ??? < 0.5mm ----
// GeoTIFF: X=0..10, Y=100..102 (EPSG:5186, 10x2m)
// ?? ??? 390?261mm ???? 2m ??? ?? ???? Y extent? ~6.69m?
// ???? raster ??? map item ??? ~92mm ???? ? FAIL ??.
// ---- [RED] raster ? ?? ? map item ?? extent < 0.5mm ----
// GeoTIFF: X=0..10, Y=100..102 (EPSG:5186, 10x2m, aspect 5:1)
// ?? ??: 390?261mm ??? ? zoomToExtent ? Y extent ~6.69m ??
// ? raster yMin/yMax ?? ~2.35m ? 0.5mm ??? ? FAIL ??.
void TestSectionLayout::buildLayoutRasterCornersAligned()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    const auto res = buildTestLayout(&proj, m_tiffPath, nullptr);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ???? ??");

    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");

    // GeoTIFF extent in EPSG:5186 (meters)
    constexpr double xMin = 0.0, xMax = 10.0, yMin = 100.0, yMax = 102.0;

    // ?? ??: 0.5mm ? ?? ?? m = 0.5 ? scale / 1000
    const double scale = mapItem->scale();
    const double tolM  = 0.5 * scale / 1000.0;

    // map->extent()? raster extent? ???? ???? ??? ??? ??? ?
    const QgsRectangle mapExt = mapItem->extent();
    QVERIFY2(std::abs(mapExt.xMinimum() - xMin) < tolM,
             qPrintable(QStringLiteral("xMin: got %1, expected %2, tol %3m (scale=%4)")
                            .arg(mapExt.xMinimum(), 0, 'f', 6).arg(xMin)
                            .arg(tolM, 0, 'f', 6).arg(scale, 0, 'f', 2)));
    QVERIFY2(std::abs(mapExt.xMaximum() - xMax) < tolM,
             qPrintable(QStringLiteral("xMax: got %1, expected %2, tol %3m")
                            .arg(mapExt.xMaximum(), 0, 'f', 6).arg(xMax).arg(tolM, 0, 'f', 6)));
    QVERIFY2(std::abs(mapExt.yMinimum() - yMin) < tolM,
             qPrintable(QStringLiteral("yMin: got %1, expected %2, tol %3m")
                            .arg(mapExt.yMinimum(), 0, 'f', 6).arg(yMin).arg(tolM, 0, 'f', 6)));
    QVERIFY2(std::abs(mapExt.yMaximum() - yMax) < tolM,
             qPrintable(QStringLiteral("yMax: got %1, expected %2, tol %3m")
                            .arg(mapExt.yMaximum(), 0, 'f', 6).arg(yMax).arg(tolM, 0, 'f', 6)));
}

// ---- [RED] scaleDenominator ??: ??? ?? ?? ?? ----
// 10x2m raster, 1:50 ? frameW=200mm, frameH=40mm (A3 390?261mm ?? ?? ? ??)
// ?? ??? scaleDenominator ??? ???? ??? ?? ??? ??? ? ? FAIL ??.
// ---- [RED] scaleDenominator ??: ??? ?? ?? ?? ----
// 10x2m raster, 1:50 ? frameW=200mm, frameH=40mm (A3 390?261mm ?? ?? ? ??)
// ?? ???? scaleDenominator ??? ?? ??? ?? ? FAIL ??.
void TestSectionLayout::buildLayoutScaleDenominator()
{
    if (m_tiffPath.isEmpty()) QSKIP("GeoTIFF ??");
    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsRasterLayer(m_tiffPath,
                                     QStringLiteral("test_section_scale"),
                                     QStringLiteral("gdal"));
    QVERIFY2(layer->isValid(), "??? ??");
    proj.addMapLayer(layer, false);

    SectionLayoutOptions opts;
    opts.paper = SectionLayoutOptions::Paper::A3;
    opts.scaleDenominator = 50.0; // 1:50 ? 10x2m ? 200?40mm, ??? ???

    const auto res = SectionLayoutService::buildSectionLayout(&proj, {layer}, opts);
    QVERIFY2(res.errorKo.isEmpty(), qPrintable(res.errorKo));

    // ?? ?? ??? 50? ???? ?? (1.0 ??)
    QVERIFY2(std::abs(res.appliedScaleDenominator - 50.0) < 1.0,
             qPrintable(QStringLiteral("appliedScaleDenominator: %1, expected ~50")
                            .arg(res.appliedScaleDenominator, 0, 'f', 2)));

    // ?? ??? ??: 200?40mm ? 1mm
    auto* ly = dynamic_cast<QgsPrintLayout*>(
        proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet ??");
    auto* mapItem = qobject_cast<QgsLayoutItemMap*>(
        ly->itemById(QStringLiteral("ka_section_map")));
    QVERIFY2(mapItem, "ka_section_map ??");
    QVERIFY2(std::abs(mapItem->rect().width()  - 200.0) < 1.0,
             qPrintable(QStringLiteral("map width: %1mm, expected ~200mm").arg(mapItem->rect().width(), 0, 'f', 2)));
    QVERIFY2(std::abs(mapItem->rect().height() -  40.0) < 1.0,
             qPrintable(QStringLiteral("map height: %1mm, expected ~40mm").arg(mapItem->rect().height(), 0, 'f', 2)));
}

#include "test_section_layout.moc"

int main(int argc, char **argv)
{
    QgsApplication app(argc, argv, false);
    const QString prefix = qEnvironmentVariable(
        "QGIS_PREFIX_PATH",
        QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
            ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
            : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
    QgsApplication::initQgis();
    TestSectionLayout tc;
    const int rc = QTest::qExec(&tc, argc, argv);
    QgsApplication::exitQgis();
    return rc;
}
