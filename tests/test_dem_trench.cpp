#include <cmath>
#include <vector>

#include <QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
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
  void buildInArea_shortAreaPlacesByCentroid();
  void startTrenchGrid_placesOnMapWithoutApplyClick();
  void pickAutoFillArea_usesNewestNotAllUnion();
  void pickAutoFillArea_usesSelectedOnly();
  void startTrenchGrid_doesNotUnionAllSurveyAreas();
  void buildForTargetRatio_hitsTenPercent();
  void buildForTargetRatio_hitsTwoPercent();
  void upslopeAspect_pointsUphillNotAlongContour();
  void upslopeAspect_flatGroundHasNoDirection();
  void buildForTargetRatio_honoursTerrainAzimuth();
  void buildInArea_trimsEdgeTrenchesInsteadOfDropping();
  void tilePack_xmlUsesXyzTopOrigin();
  void tilePack_tileCountMatchesWebMercator();
  void layerTreeMenu_hasLabelToggleAndTrenchRatio();
  void applySnapConfig_vertexAndSegmentNotWmsPromise();
  void trenchWholeMove_commitsOnMouseRelease();
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

// 시굴조사 도메인: 트렌치는 조사구역 안에 완전히 들어가야 하고, 구역 전체에
// 고르게 깔려야 한다. 100×100에 12×30 격자면 3줄은 통째로, 마지막 줄은 잘려서
// 들어간다(예전에는 마지막 줄을 통째로 버려 위쪽 30 m가 비었다).
void TestDemTrench::buildInAreaKeepsCellsInsideAndRatio() {
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 10.0;  // step 12 × 30
  s.azimuthDeg = 0.0;
  const QByteArray area = squareWkb(0.0, 0.0, 100.0, 100.0);
  const auto cells = TrenchGridGenerator::buildInArea(s, area);
  QCOMPARE(static_cast<int>(cells.size()), 36);  // 9칸 × (온전한 3줄 + 잘린 1줄)
  int full = 0, trimmed = 0;
  double topMost = -1e300;
  for (const auto& c : cells) {
    QCOMPARE(c.width, 2.0);  // 폭 2 m는 절대 변하지 않는다
    if (std::abs(c.length - 20.0) < 1e-6) ++full; else ++trimmed;
    for (const auto& pt : c.ring) {
      QVERIFY2(pt.first >= -1e-9 && pt.first <= 100.0 + 1e-9, "x out of area");
      QVERIFY2(pt.second >= -1e-9 && pt.second <= 100.0 + 1e-9, "y out of area");
      topMost = std::max(topMost, pt.second);
    }
  }
  QCOMPARE(full, 27);
  QCOMPARE(trimmed, 9);
  QVERIFY2(topMost > 99.0, "위쪽 가장자리까지 덮어야 고르다");
  const double total = TrenchGridGenerator::totalArea(cells);
  QVERIFY2(std::abs(total - (27 * 40.0 + 9 * 20.0)) < 0.01, qPrintable(QString::number(total)));
  const double pct = total / (100.0 * 100.0) * 100.0;
  QVERIFY2(pct > 12.0 && pct < 13.0, qPrintable(QString::number(pct)));  // 12.6%
}

// 2×20 m 트렌치가 구역 한 변보다 길면 Contains는 0개. 중심이 구역 안이면 깔아야 한다.
void TestDemTrench::buildInArea_shortAreaPlacesByCentroid() {
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 1.0;
  s.azimuthDeg = 0.0;
  const QByteArray area = squareWkb(0.0, 0.0, 12.0, 18.0);
  const auto cells = TrenchGridGenerator::buildInArea(s, area);
  QVERIFY2(!cells.empty(), "짧은 조사구역에도 시굴격자가 생겨야 한다");
  for (const auto& c : cells) {
    const double cx = (c.ring[0].first + c.ring[2].first) * 0.5;
    const double cy = (c.ring[0].second + c.ring[2].second) * 0.5;
    QVERIFY2(cx >= -1e-6 && cx <= 12.0 + 1e-6 && cy >= -1e-6 && cy <= 18.0 + 1e-6,
             "centroid must stay in the survey area");
  }
}

// 예전 조사구역이 남아 있어도 자동 배치는 마지막(또는 선택한) 구역만 쓴다.
void TestDemTrench::pickAutoFillArea_usesNewestNotAllUnion() {
  const auto oldBig = squareWkb(0.0, 0.0, 1000.0, 1000.0);
  const auto newSmall = squareWkb(0.0, 0.0, 20.0, 20.0);
  const std::vector<TrenchGridGenerator::SurveyPoly> feats{{oldBig, 1}, {newSmall, 2}};
  const auto pick = TrenchGridGenerator::pickAutoFillArea(feats, {});
  QVERIFY2(!pick.usedSelection, "선택이 없으면 마지막 구역만");
  QCOMPARE(pick.usedCount, 1);
  QCOMPARE(pick.totalCount, 2);
  QVERIFY2(std::abs(pick.areaM2 - 400.0) < 1.0, qPrintable(QString::number(pick.areaM2)));
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 10.0;
  s.azimuthDeg = 0.0;
  const auto cells = TrenchGridGenerator::buildInArea(s, pick.wkb);
  QVERIFY2(!cells.empty(), "새 조사구역 안에도 격자가 생겨야 한다");
  QVERIFY2(static_cast<int>(cells.size()) < 20, "옛 구역 유니온이면 수백 칸이 된다");
  for (const auto& c : cells) {
    for (const auto& pt : c.ring) {
      QVERIFY2(pt.first <= 20.0 + 1e-6 && pt.second <= 20.0 + 1e-6, "cell outside newest area");
    }
  }
}

void TestDemTrench::pickAutoFillArea_usesSelectedOnly() {
  const auto oldBig = squareWkb(0.0, 0.0, 100.0, 100.0);
  const auto newSmall = squareWkb(200.0, 200.0, 220.0, 220.0);
  const std::vector<TrenchGridGenerator::SurveyPoly> feats{{oldBig, 1}, {newSmall, 2}};
  const auto pick = TrenchGridGenerator::pickAutoFillArea(feats, {1});
  QVERIFY(pick.usedSelection);
  QCOMPARE(pick.usedCount, 1);
  QVERIFY2(std::abs(pick.areaM2 - 10000.0) < 1.0, qPrintable(QString::number(pick.areaM2)));
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 10.0;
  const auto cells = TrenchGridGenerator::buildInArea(s, pick.wkb);
  QVERIFY(!cells.empty());
  for (const auto& c : cells) {
    const double cx = (c.ring[0].first + c.ring[2].first) * 0.5;
    const double cy = (c.ring[0].second + c.ring[2].second) * 0.5;
    QVERIFY2(cx >= -1e-6 && cx <= 100.0 + 1e-6 && cy >= -1e-6 && cy <= 100.0 + 1e-6,
             "selected old area only");
  }
}

void TestDemTrench::startTrenchGrid_doesNotUnionAllSurveyAreas() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int start = src.indexOf(QLatin1String("void MainWindow::startTrenchGrid()"));
  const int next = src.indexOf(QLatin1String("bool MainWindow::applyTrenchFromDialog()"));
  QVERIFY2(start >= 0 && next > start, "startTrenchGrid");
  const QString fn = src.mid(start, next - start);
  QVERIFY2(src.contains(QLatin1String("pickAutoFillArea")),
           "시굴격자는 survey_area 전체를 combine 하지 말고 pickAutoFillArea를 쓴다");
  QVERIFY2(fn.contains(QLatin1String("trenchFillFromSurveyLayer")) ||
               fn.contains(QLatin1String("pickAutoFillArea")),
           "startTrenchGrid는 leftover union 대신 고른 구역만 써야 한다");
  QVERIFY2(!fn.contains(QLatin1String("uni.combine")),
           "남은 조사구역을 union 하면 옛 구역에 격자가 깔린다");
}

void TestDemTrench::buildForTargetRatio_hitsTenPercent() {
  const QByteArray area = squareWkb(0.0, 0.0, 100.0, 100.0);
  const auto plan = TrenchGridGenerator::buildForTargetRatio(area, 10.0, 2.0);
  QVERIFY2(!plan.cells.empty(), "시굴 10% 격자가 비면 안 된다");
  for (const auto& c : plan.cells) {
    QVERIFY2(std::abs(c.width - 2.0) < 1e-9, "폭은 2 m 고정");
    QVERIFY2(c.length > 0.0, "길이를 배분해야 한다");
    for (const auto& pt : c.ring) {
      QVERIFY2(pt.first >= -1e-6 && pt.first <= 100.0 + 1e-6, "x out of area");
      QVERIFY2(pt.second >= -1e-6 && pt.second <= 100.0 + 1e-6, "y out of area");
    }
  }
  const double pct = TrenchGridGenerator::totalArea(plan.cells) / 10000.0 * 100.0;
  QVERIFY2(pct >= 8.5 && pct <= 11.5, qPrintable(QStringLiteral("시굴 %1%").arg(pct)));
}

void TestDemTrench::buildForTargetRatio_hitsTwoPercent() {
  const QByteArray area = squareWkb(0.0, 0.0, 100.0, 100.0);
  const auto plan = TrenchGridGenerator::buildForTargetRatio(area, 2.0, 2.0);
  QVERIFY2(!plan.cells.empty(), "표본 2% 격자가 비면 안 된다");
  for (const auto& c : plan.cells)
    QVERIFY2(std::abs(c.width - 2.0) < 1e-9, "폭은 2 m 고정");
  const double pct = TrenchGridGenerator::totalArea(plan.cells) / 10000.0 * 100.0;
  QVERIFY2(pct >= 1.2 && pct <= 2.8, qPrintable(QStringLiteral("표본 %1%").arg(pct)));
  QVERIFY2(pct < 8.0, "표본은 시굴(10%)보다 훨씬 적어야 한다");
}

// 사면이 동쪽으로 올라가면 트렌치 장축은 동쪽(방위 90°)을 봐야 한다.
// 등고선은 남북으로 서 있으므로, 90°가 곧 등고선 직교다.
void TestDemTrench::upslopeAspect_pointsUphillNotAlongContour() {
  std::vector<TrenchGridGenerator::ElevSample> samples;
  for (int i = 0; i <= 10; ++i) {
    for (int j = 0; j <= 10; ++j) {
      const double x = 200000.0 + i * 10.0;
      const double y = 450000.0 + j * 10.0;
      samples.push_back({x, y, 0.05 * (x - 200000.0)});  // 동쪽으로 5% 오르막
    }
  }
  const auto asp = TrenchGridGenerator::upslopeAspect(samples);
  QVERIFY2(asp.valid, "5% 사면은 방향이 잡혀야 한다");
  QVERIFY2(std::abs(asp.azimuthDeg - 90.0) < 0.5,
           qPrintable(QStringLiteral("동쪽 오르막 → 90°, 실제 %1").arg(asp.azimuthDeg)));
  QVERIFY2(std::abs(asp.slopePct - 5.0) < 0.1, "경사 5%");

  // 북동쪽으로 올라가면 45°.
  std::vector<TrenchGridGenerator::ElevSample> ne;
  for (int i = 0; i <= 10; ++i)
    for (int j = 0; j <= 10; ++j) {
      const double x = 200000.0 + i * 10.0;
      const double y = 450000.0 + j * 10.0;
      ne.push_back({x, y, 0.03 * (x - 200000.0) + 0.03 * (y - 450000.0)});
    }
  const auto asp2 = TrenchGridGenerator::upslopeAspect(ne);
  QVERIFY(asp2.valid);
  QVERIFY2(std::abs(asp2.azimuthDeg - 45.0) < 0.5,
           qPrintable(QStringLiteral("북동 오르막 → 45°, 실제 %1").arg(asp2.azimuthDeg)));
}

// 평지에서는 방향을 지어내면 안 된다. 표본이 3개 미만일 때도 마찬가지.
void TestDemTrench::upslopeAspect_flatGroundHasNoDirection() {
  std::vector<TrenchGridGenerator::ElevSample> flat;
  for (int i = 0; i <= 6; ++i)
    for (int j = 0; j <= 6; ++j)
      flat.push_back({200000.0 + i * 10.0, 450000.0 + j * 10.0, 31.4});
  QVERIFY2(!TrenchGridGenerator::upslopeAspect(flat).valid, "평지는 valid=false");

  const std::vector<TrenchGridGenerator::ElevSample> two = {{0, 0, 0}, {10, 0, 1}};
  QVERIFY2(!TrenchGridGenerator::upslopeAspect(two).valid, "표본 2개는 valid=false");
}

// 지형 방위를 주면 그 방위로 깔리고, 비율은 그대로 지켜야 한다.
void TestDemTrench::buildForTargetRatio_honoursTerrainAzimuth() {
  const QByteArray area = squareWkb(200000.0, 450000.0, 200200.0, 450200.0);
  const auto plan = TrenchGridGenerator::buildForTargetRatio(area, 10.0, 2.0, 45.0);
  QVERIFY(!plan.cells.empty());
  QCOMPARE(plan.azimuthDeg, 45.0);
  QVERIFY2(std::abs(plan.ratioPct - 10.0) < 1.5,
           qPrintable(QStringLiteral("시굴 10%% 근처여야 함, 실제 %1").arg(plan.ratioPct)));

  // 첫 트렌치의 장축이 45°를 향하는지 — ring[0]→ring[3]이 길이 방향이다.
  const auto& c = plan.cells.front();
  const double dx = c.ring[3].first - c.ring[0].first;
  const double dy = c.ring[3].second - c.ring[0].second;
  double az = std::atan2(dx, dy) * 180.0 / M_PI;
  if (az < 0.0) az += 360.0;
  QVERIFY2(std::abs(az - 45.0) < 0.5,
           qPrintable(QStringLiteral("장축 방위 45°여야 함, 실제 %1").arg(az)));
}

// 고르게 깔리려면 경계에 걸친 트렌치를 버리지 말고 길이를 잘라 남겨야 한다.
// 버리면 가장자리가 비어 배치가 한쪽으로 쏠린다.
void TestDemTrench::buildInArea_trimsEdgeTrenchesInsteadOfDropping() {
  // 폭 45 m 세로 띠 — 20 m 트렌치가 격자에 딱 떨어지지 않아 가장자리가 남는다.
  const QByteArray area = squareWkb(200000.0, 450000.0, 200045.0, 450045.0);
  TrenchGridGenerator::Spec s;
  s.trenchWidth = 2.0;
  s.trenchLength = 20.0;
  s.balkWidth = 6.0;
  s.azimuthDeg = 0.0;

  const auto cells = TrenchGridGenerator::buildInArea(s, area);
  QVERIFY2(!cells.empty(), "트렌치가 나와야 한다");

  // 잘린 트렌치가 생겨야 정상(전부 20 m면 가장자리를 버린 것).
  bool sawTrimmed = false;
  for (const auto& c : cells) {
    QVERIFY2(c.width == 2.0, "폭은 2 m 고정");
    QVERIFY2(c.length >= 4.0 - 1e-9, "너무 짧은 조각은 버려야 한다");
    QVERIFY2(c.length <= 20.0 + 1e-9, "규격보다 길어질 수 없다");
    if (c.length < 20.0 - 1e-6) sawTrimmed = true;
    // 잘렸든 아니든 구역 밖으로 나가면 안 된다.
    for (const auto& pt : c.ring) {
      QVERIFY2(pt.first >= 200000.0 - 1e-6 && pt.first <= 200045.0 + 1e-6, "x가 구역 안");
      QVERIFY2(pt.second >= 450000.0 - 1e-6 && pt.second <= 450045.0 + 1e-6, "y가 구역 안");
    }
  }
  QVERIFY2(sawTrimmed, "가장자리 트렌치는 잘려서라도 남아야 한다");

  // 세로 방향 덮임이 구역 위아래로 고르게 퍼져야 한다.
  double topMost = -1e300, botMost = 1e300;
  for (const auto& c : cells)
    for (const auto& pt : c.ring) {
      topMost = std::max(topMost, pt.second);
      botMost = std::min(botMost, pt.second);
    }
  QVERIFY2(botMost < 450004.0, "아래 가장자리까지 내려가야 한다");
  QVERIFY2(topMost > 450041.0, "위 가장자리까지 올라가야 한다");
}

// GDAL TMS 미니드라이버는 ${z} 표기를 쓰고, XYZ는 위가 원점이다.
// YOrigin을 빼먹으면 남북이 뒤집힌 배경지도가 저장된다.
void TestDemTrench::tilePack_xmlUsesXyzTopOrigin() {
  TilePackService::Options o;
  o.urlTemplate = QStringLiteral("https://example.kr/wmts/{z}/{y}/{x}.jpeg");
  o.minZoom = 12;
  o.maxZoom = 18;
  const QString xml = TilePackService::serviceXml(o);

  QVERIFY2(xml.contains(QLatin1String("${z}/${y}/${x}")), "GDAL 표기로 바뀌어야 함");
  QVERIFY2(!xml.contains(QLatin1String("{z}/{y}/{x}")), "QGIS 표기가 남으면 안 됨");
  QVERIFY2(xml.contains(QLatin1String("<YOrigin>top</YOrigin>")), "XYZ는 위가 원점");
  QVERIFY2(xml.contains(QLatin1String("<TileLevel>18</TileLevel>")), "최대 줌이 들어가야 함");
  QVERIFY2(xml.contains(QLatin1String("EPSG:3857")), "웹메르카토르");
  QVERIFY2(xml.contains(QLatin1String("<Service name=\"TMS\"")), "TMS 미니드라이버");
}

// 내려받기 전에 "몇 장"을 알려 줘야 사용자가 기다릴지 판단한다.
void TestDemTrench::tilePack_tileCountMatchesWebMercator() {
  const double half = TilePackService::webMercatorHalfWorld();
  QVERIFY2(std::abs(half - 20037508.342789244) < 1e-6, "웹메르카토르 반폭");

  // 줌 0은 세계가 타일 한 장. 픽셀 하나가 약 156,543 m.
  QVERIFY2(std::abs(TilePackService::resolutionAtZoom(0) - 156543.03392) < 0.01,
           qPrintable(QString::number(TilePackService::resolutionAtZoom(0))));
  // 줌이 하나 오르면 해상도는 절반.
  QVERIFY2(std::abs(TilePackService::resolutionAtZoom(10) -
                    TilePackService::resolutionAtZoom(9) / 2.0) < 1e-9,
           "줌 한 단계 = 해상도 절반");

  // 세계 전체를 줌 0으로 = 1장, 줌 0~1 = 1 + 4 = 5장.
  QCOMPARE(TilePackService::tileCount(-half + 1, -half + 1, half - 1, half - 1, 0, 0),
           static_cast<qint64>(1));
  QCOMPARE(TilePackService::tileCount(-half + 1, -half + 1, half - 1, half - 1, 0, 1),
           static_cast<qint64>(5));

  // 빈 범위는 0장.
  QCOMPARE(TilePackService::tileCount(0, 0, 0, 0, 12, 18), static_cast<qint64>(0));

  // 조사구역 크기(한 변 1 km)를 12~18로 덮으면 현실적인 장수여야 한다.
  const qint64 n = TilePackService::tileCount(14100000.0, 4500000.0, 14101000.0, 4501000.0, 12, 18);
  QVERIFY2(n > 0 && n < 400, qPrintable(QStringLiteral("1km 구역 12~18 → %1장").arg(n)));
}

void TestDemTrench::layerTreeMenu_hasLabelToggleAndTrenchRatio() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int start = src.indexOf(QLatin1String("void MainWindow::onLayerTreeContextMenu"));
  const int next = src.indexOf(QLatin1String("void MainWindow::renameSelectedLayer"));
  QVERIFY2(start >= 0 && next > start, "onLayerTreeContextMenu");
  const QString fn = src.mid(start, next - start);
  QVERIFY2(fn.contains(QStringLiteral("글자")), "레이어 우클릭에 글자 켜기/끄기가 있어야 한다");
  QVERIFY2(fn.contains(QStringLiteral("시굴격자")), "조사구역 우클릭에 시굴격자 메뉴가 있어야 한다");
  QVERIFY2(fn.contains(QStringLiteral("시굴")) && fn.contains(QStringLiteral("표본")),
           "시굴·표본 두 항목");
  QVERIFY2(src.contains(QLatin1String("applyTrenchByRatio")) ||
               src.contains(QLatin1String("buildForTargetRatio")),
           "10%/2%는 길이 배분으로 자동 배치해야 한다");
  QVERIFY2(src.contains(QLatin1String("setLabelsVisible")),
           "글자 토글은 LayerOps::setLabelsVisible");
}

void TestDemTrench::applySnapConfig_vertexAndSegmentNotWmsPromise() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int snap = src.indexOf(QLatin1String("void MainWindow::applySnapConfig()"));
  QVERIFY2(snap >= 0, "applySnapConfig");
  const QString fn = src.mid(snap, 700);
  QVERIFY2(fn.contains(QLatin1String("SnappingType::Vertex")), "꼭짓점 자석");
  QVERIFY2(fn.contains(QLatin1String("SnappingType::Segment")),
           "선에도 붙어야 조사구역·SHP 그리기가 편하다");
  QVERIFY2(src.contains(QStringLiteral("위성·지적 그림")),
           "지적 WMS는 그림이라 자석이 안 붙는다고 안내해야 한다");
}

void TestDemTrench::startTrenchGrid_placesOnMapWithoutApplyClick() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int start = src.indexOf(QLatin1String("void MainWindow::startTrenchGrid()"));
  const int next = src.indexOf(QLatin1String("bool MainWindow::applyTrenchFromDialog()"));
  QVERIFY2(start >= 0 && next > start, "startTrenchGrid");
  const QString fn = src.mid(start, next - start);
  const int shown = fn.indexOf(QLatin1String("m_trenchDlg->show()"));
  QVERIFY2(shown >= 0, "startTrenchGrid shows the adjust panel");
  const QString afterShow = fn.mid(shown);
  QVERIFY2(afterShow.contains(QLatin1String("applyTrenchFromDialog()")) ||
               afterShow.contains(QLatin1String("beginTrenchOriginPick()")),
           "시굴격자는 속성 창만 띄우지 말고 바로 맵에 깔거나 원점을 찍게 해야 한다");
}

void TestDemTrench::trenchWholeMove_commitsOnMouseRelease() {
  QFile f(QStringLiteral("src/app/KaTrenchMoveTool.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaTrenchMoveTool.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int rel = src.indexOf(QLatin1String("void KaTrenchMoveTool::canvasReleaseEvent"));
  QVERIFY2(rel >= 0, "canvasReleaseEvent");
  const QString fn = src.mid(rel, 900);
  QVERIFY2(fn.contains(QLatin1String("applyTranslate(")),
           "전체 이동은 끌어다 놓으면 격자 전체가 옮겨져야 한다");
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
