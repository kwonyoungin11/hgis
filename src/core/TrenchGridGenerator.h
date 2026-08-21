#pragma once

#include <QByteArray>
#include <QString>
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

// Fills the survey-area polygon (WKB) with trenches: a rotated regular grid
// anchored at the polygon envelope centre; only cells fully inside the area
// are kept. rows/cols/origin of the spec are ignored.
std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb);

// Sum of trench areas (w × len per cell, square metres).
double totalArea(const std::vector<Cell>& cells);

bool writeGpkg(const QString& gpkgPath, const QString& layerName, const std::vector<Cell>& cells,
               const QString& authid, QString* errorOut);

// Deletes every feature of the layer (no-op when file/layer is missing).
// "새로 만들기" must replace the previous grid instead of stacking on top of it.
bool clearLayer(const QString& gpkgPath, const QString& layerName, QString* errorOut);

}  // namespace TrenchGridGenerator
