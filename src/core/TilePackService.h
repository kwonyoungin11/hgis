#pragma once

#include <QString>
#include <QtGlobal>

// 조사구역 범위의 배경지도 타일을 MBTiles(SQLite) 한 파일로 미리 받아 둔다.
//
// 왜: 위성·지적이 원격 WMTS라 화면을 옮길 때마다 타일을 네트워크로 받는다.
// 4K에서 한 장 그리는 데 2.4초가 걸렸다(2026-09-04 실측). 조사구역만 미리
// 받아 두면 그 뒤로는 디스크에서 읽어 네트워크를 아예 타지 않는다.
// 현장 LTE가 끊겨도 배경지도가 뜬다.
//
// MBTiles는 SQLite라 조사 GPKG 옆에 파일 하나로 같이 복사된다.
// 참조 지도일 뿐 조사 도메인이 아니다 — 제출(5179) 대상이 아니다.
namespace TilePackService {

struct Options {
  // QGIS XYZ 레이어와 같은 자리표시자를 쓴다: {z} {x} {y}
  QString urlTemplate;
  int minZoom = 12;
  int maxZoom = 18;
  int bandCount = 3;
  // JPEG는 위성처럼 사진일 때, PNG는 지적처럼 선·글자일 때.
  bool jpeg = true;
  QString referer;
};

// GDAL WMS(TMS 미니드라이버) 설정 XML. 네트워크 없이 만들 수 있어 검사 가능.
QString serviceXml(const Options& opt);

// EPSG:3857 한 변의 절반(m). 웹메르카토르 타일 계산의 기준.
double webMercatorHalfWorld();

// 줌 z에서 픽셀 하나가 덮는 거리(m). 타일은 256 px 기준.
double resolutionAtZoom(int z);

// bbox(EPSG:3857)를 minZoom~maxZoom으로 덮는 데 필요한 타일 장수.
// 내려받기 전에 "몇 장 · 얼마나 걸림"을 사용자에게 알려 주려고 쓴다.
qint64 tileCount(double minX, double minY, double maxX, double maxY, int minZoom, int maxZoom);

// 실제로 받아 MBTiles로 쓴다. 네트워크가 필요하다.
bool build(const Options& opt, double minX, double minY, double maxX, double maxY,
           const QString& outPath, QString* errorOut);

}  // namespace TilePackService
