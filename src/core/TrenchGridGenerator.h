#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <array>
#include <vector>

// Rotated excavation trench rectangles in work CRS (5186/5187). GPKG via OGR.
//
// 시굴조사 도메인: 트렌치는 조사구역 폴리곤 전체를 일정 간격으로 덮고,
// 총 굴착 면적이 구역 면적의 규정 비율(시굴 10%, 표본 2%) 이내여야 한다.
namespace TrenchGridGenerator {

struct Spec {
  double originX = 0.0;
  double originY = 0.0;
  double trenchWidth = 2.0;
  double trenchLength = 20.0;
  double balkWidth = 1.0;
  int rows = 1;
  int cols = 1;
  double azimuthDeg = 0.0;  // clockwise from north
  QString namePrefix = QStringLiteral("Tr-");
};

struct Cell {
  QString name;
  double width = 2.0;
  double length = 20.0;
  // Closed ring, 5 points, work CRS metres.
  std::array<std::pair<double, double>, 5> ring{};
};

std::vector<Cell> build(const Spec& spec);

// One survey_area feature (work CRS WKB). fid is QGIS/OGR id; newer draws
// usually have a higher fid.
struct SurveyPoly {
  QByteArray wkb;
  qint64 fid = -1;
};

struct PickedArea {
  QByteArray wkb;
  double areaM2 = 0.0;
  int usedCount = 0;
  int totalCount = 0;
  bool usedSelection = false;
};

// Auto-fill must not union leftover survey polygons. Selected fids (if any
// match) are combined; otherwise only the highest fid (last drawn) is used.
PickedArea pickAutoFillArea(const std::vector<SurveyPoly>& features,
                            const std::vector<qint64>& selectedFids);

// Fills the survey-area polygon (WKB) with trenches: a rotated regular grid
// anchored at the polygon envelope centre. Prefer cells fully inside; if none
// fit (trench longer than the area), keep cells whose centroid is inside.
// rows/cols/origin of the spec are ignored.
std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb);

// Width stays 2 m. Length and balk are searched so total trench area / survey
// area ≈ targetPct (시굴 10, 표본 2). Cells stay inside the picked polygon.
struct RatioFill {
  std::vector<Cell> cells;
  double length = 20.0;
  double balk = 10.0;
  double ratioPct = 0.0;
  double areaM2 = 0.0;
};
RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width = 2.0);

// Sum of trench areas (w × len per cell, square metres).
double totalArea(const std::vector<Cell>& cells);

bool writeGpkg(const QString& gpkgPath, const QString& layerName, const std::vector<Cell>& cells,
               const QString& authid, QString* errorOut);

// Deletes every feature of the layer (no-op when file/layer is missing).
// "새로 만들기" must replace the previous grid instead of stacking on top of it.
bool clearLayer(const QString& gpkgPath, const QString& layerName, QString* errorOut);

}  // namespace TrenchGridGenerator
