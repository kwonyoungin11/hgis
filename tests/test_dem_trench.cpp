#include <cmath>
#include <vector>

#include <QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "core/DemAnalyzer.h"
#include "core/TrenchGridGenerator.h"
#include "core/CanvasGridMath.h"

#include <gdal_priv.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>

class TestDemTrench : public QObject {
  Q_OBJECT
private slots:
  void trench1x1Is40m2();
  void trenchTwoColsSpacedByBalk();
  void buildInAreaKeepsCellsInsideAndRatio();
  void clearLayerReplacesPreviousGrid();
  void hillshadeWritesByteTif();
  void niceMeterStepAround120px();
};

namespace {
QByteArray squareWkb(double x0, double y0, double x1, double y1) {
  OGRLinearRing ring;
  ring.addPoint(x0, y0);
  ring.addPoint(x1, y0);
  ring.addPoint(x1, y1);
  ring.addPoint(x0, y1);
  ring.addPoint(x0, y0);
  OGRPolygon poly;
  poly.addRing(&ring);
  QByteArray wkb(static_cast<int>(poly.WkbSize()), '\0');
  poly.exportToWkb(reinterpret_cast<unsigned char*>(wkb.data()));
  return wkb;
}
}  // namespace

void TestDemTrench::trench1x1Is40m2() {
  TrenchGridGenerator::Spec s;
  s.originX = 200000.0;
  s.originY = 450000.0;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 1.0;
  s.rows = 1;
  s.cols = 1;
  s.azimuthDeg = 0.0;
  const auto cells = TrenchGridGenerator::build(s);
  QCOMPARE(cells.size(), 1);
  QCOMPARE(cells[0].name, QStringLiteral("Tr-1"));
  double area = 0.0;
  for (int i = 0; i < 4; ++i) {
    const auto& a = cells[0].ring[i];
    const auto& b = cells[0].ring[i + 1];
    area += a.first * b.second - b.first * a.second;
  }
  area = std::abs(area) * 0.5;
  QVERIFY2(std::abs(area - 40.0) < 0.01, qPrintable(QString::number(area)));
}

void TestDemTrench::trenchTwoColsSpacedByBalk() {
  TrenchGridGenerator::Spec s;
  s.originX = 0.0;
  s.originY = 0.0;
  s.trenchWidth = 2.0;
  s.trenchLength = 10.0;
  s.balkWidth = 1.0;
  s.rows = 1;
  s.cols = 2;
  s.azimuthDeg = 0.0;
  const auto cells = TrenchGridGenerator::build(s);
  QCOMPARE(cells.size(), 2);
  QVERIFY(std::abs(cells[1].ring[0].first - 3.0) < 1e-9);
}

// 시굴조사 도메인: 트렌치는 조사구역 안에 완전히 들어가야 하고,
// 총면적/구역면적 비율이 계획 수치(예: 10%)로 계산되어야 한다.
void TestDemTrench::buildInAreaKeepsCellsInsideAndRatio() {
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 10.0;  // step 12 × 30
  s.azimuthDeg = 0.0;
  const QByteArray area = squareWkb(0.0, 0.0, 100.0, 100.0);
  const auto cells = TrenchGridGenerator::buildInArea(s, area);
  QCOMPARE(static_cast<int>(cells.size()), 27);  // 9 cols × 3 rows
  for (const auto& c : cells) {
    for (const auto& pt : c.ring) {
      QVERIFY2(pt.first >= -1e-9 && pt.first <= 100.0 + 1e-9, "x out of area");
      QVERIFY2(pt.second >= -1e-9 && pt.second <= 100.0 + 1e-9, "y out of area");
    }
  }
  const double total = TrenchGridGenerator::totalArea(cells);
  QVERIFY2(std::abs(total - 27 * 40.0) < 0.01, qPrintable(QString::number(total)));
  const double pct = total / (100.0 * 100.0) * 100.0;
  QVERIFY2(pct > 10.0 && pct < 11.0, qPrintable(QString::number(pct)));  // 10.8%
}

// "새로 만들기"는 이전 격자를 대체해야 한다(겹침 금지).
void TestDemTrench::clearLayerReplacesPreviousGrid() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString gpkg = tmp.filePath(QStringLiteral("survey.gpkg"));
  // writeGpkg는 「새 조사」가 만든 기존 GPKG를 여는 방식 → 빈 GPKG를 먼저 생성.
  GDALAllRegister();
  GDALDriver* gpkgDrv = GetGDALDriverManager()->GetDriverByName("GPKG");
  QVERIFY(gpkgDrv);
  GDALDataset* seed =
      gpkgDrv->Create(gpkg.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
  QVERIFY(seed);
  GDALClose(seed);
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 10.0;
  s.rows = 1;
  s.cols = 2;
  QString err;
  QVERIFY2(TrenchGridGenerator::writeGpkg(gpkg, QStringLiteral("trial_trench"),
                                          TrenchGridGenerator::build(s),
                                          QStringLiteral("EPSG:5186"), &err),
           qPrintable(err));
  QVERIFY2(TrenchGridGenerator::clearLayer(gpkg, QStringLiteral("trial_trench"), &err),
           qPrintable(err));
  s.cols = 1;
  QVERIFY2(TrenchGridGenerator::writeGpkg(gpkg, QStringLiteral("trial_trench"),
                                          TrenchGridGenerator::build(s),
                                          QStringLiteral("EPSG:5186"), &err),
           qPrintable(err));
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx(gpkg.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
  QVERIFY(ds);
  OGRLayer* lyr = ds->GetLayerByName("trial_trench");
  QVERIFY(lyr);
  QCOMPARE(static_cast<int>(lyr->GetFeatureCount()), 1);
  GDALClose(ds);
}

void TestDemTrench::hillshadeWritesByteTif() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString src = tmp.filePath(QStringLiteral("dem.tif"));
  const QString dst = tmp.filePath(QStringLiteral("hs.tif"));
  GDALAllRegister();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY(drv);
  GDALDataset* ds = drv->Create(src.toUtf8().constData(), 8, 8, 1, GDT_Float32, nullptr);
  QVERIFY(ds);
  double gt[6] = {0, 1, 0, 8, 0, -1};
  ds->SetGeoTransform(gt);
  std::vector<float> z(64);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      z[y * 8 + x] = static_cast<float>(x);
  ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 8, 8, z.data(), 8, 8, GDT_Float32, 0, 0);
  GDALClose(ds);
  QString err;
  QVERIFY2(DemAnalyzer::runHillshadeFile(src, dst, DemAnalyzer::Options{}, &err), qPrintable(err));
  QVERIFY(QFile::exists(dst));
}

void TestDemTrench::niceMeterStepAround120px() {
  QCOMPARE(CanvasGridMath::niceStepMeters(120.0), 100.0);
  QCOMPARE(CanvasGridMath::niceStepMeters(8.0), 10.0);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestDemTrench tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_dem_trench.moc"
