#pragma once

#include <QImage>
#include <QString>
#include <QVector>
#include <vector>

// DEM 높이 메시 + 위성 텍스처 3D. 참조 전용 — 제출 도메인/5179 아님.
namespace Terrain3dService {

struct DemScene {
  int width = 0;
  int height = 0;
  double geotransform[6] = {0, 1, 0, 0, 0, -1};
  std::vector<float> z;
  float zMin = 0;
  float zMax = 0;
  double groundWidthM = 0;
  double groundHeightM = 0;
  double centerX = 0;
  double centerY = 0;
  QString projectionWkt;
};

struct Mesh {
  QVector<float> positions;  // x,y,z meters, origin = DEM center
  QVector<float> uvs;
  QVector<unsigned int> indices;
  float zMin = 0;
  float zMax = 0;
  double groundWidthM = 0;
  double groundHeightM = 0;
};

bool loadDem(const QString& path, DemScene* out, QString* errorOut = nullptr);
bool loadDemClip(const QString& path, double xMin, double yMin, double xMax, double yMax,
                 int maxEdge, DemScene* out, QString* errorOut = nullptr);
Mesh buildMesh(const DemScene& scene, int maxEdge, float zExaggeration);
QImage hillshadeTexture(const DemScene& scene);
QImage renderPerspective(const Mesh& mesh, const QImage& texture, int pixelW, int pixelH,
                         float yawDeg, float pitchDeg, float distance);
// 투시 화면 가운데에서 보이는 지상 폭(m). 줌·거리와 같이 변한다.
double visibleWidthAtTarget(float distance, int pixelW, int pixelH);
double distanceForVisibleWidth(double visibleWidthM, int pixelW, int pixelH);
double scaleBarSegmentM(double visibleWidthM);
// 격자 +Y가 지도 북이 아닐 때(회전 geotransform) 북 방위(도, +X=동 기준 atan2).
double northAzimuthDeg(const double geotransform[6]);

struct DemCandidate {
  QString name;
  QString source;
};
// 로컬 DEM만. 이름에 DEM/고도/표고가 있으면 우선. 애매하면 빈 문자열.
QString pickLocalDemSource(const QVector<DemCandidate>& cands);
// 로컬 이름 DEM 우선. 없으면 툴바 Copernicus(/vsicurl)도 허용.
QString pickTerrainDemSource(const QVector<DemCandidate>& cands);

// 3D 화면 아래에 축척자·방위(North)를 붙인 출력 이미지.
QImage composeExport(const QImage& view, double visibleWidthM, double yawDegFromNorth);
// 입체지형 도면: 범례·축척자·방위·좌표계.
QImage composeSheet(const QImage& view, double visibleWidthM, double yawDegFromNorth,
                    const QString& crsLabel, const QString& demName, float zMin, float zMax);
QString googleSatelliteXyzUri();
bool textureLooksFilled(const QImage& img);
// DEM 네 꼭짓점(회전 geotransform 포함)의 축정렬 세계 좌표 상자.
bool demWorldRect(const DemScene& sc, double* xMin, double* yMin, double* xMax, double* yMax);

}  // namespace Terrain3dService
