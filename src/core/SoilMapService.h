#pragma once

#include <QColor>
#include <QString>
#include <QUrl>

class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// 흙토람(농촌진흥청 국립농업과학원) 공개 GeoServer에서 정밀토양도 폴리곤을
// WFS로 내려받아 로컬 GPKG로 저장하고, 분포지형(soil_type_geo)을 흙토람 공식
// 범례색으로 표시한다.
//
//  - 서버: https://gis.naas.go.kr/geoserver/soilmap (키·신청 불필요, EPSG:5186 네이티브)
//  - SOIL_1·2·3은 같은 스키마. 같은 화면 bbox에도 여러 테이블이 같이 있다.
//    범례는 soil_type_geo만 쓰므로 그 필드만 요청한다.
//  - 내려받기 범위는 현재 화면(작업 CRS EPSG:5186). 80 km는 거부 상한이다.
//  - 한 번 내려받으면 GPKG로 남아 오프라인 현장에서도 쓸 수 있다.
class SoilMapService {
public:
  // extent(EPSG:5186) 화면 범위의 토양 폴리곤을 내려받아 outGpkgPath에 저장하고
  // 프로젝트 「참조 지도」 그룹에 추가한다. 성공 시 추가된 레이어를 반환.
  // 한 변이 maxSpanMeters를 넘으면 거부한다. 화면을 80 km로 키우지 않는다.
  static QgsVectorLayer* downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                        const QgsRectangle& extent5186,
                                        const QString& outGpkgPath,
                                        QString* errorOut = nullptr);

  // 분포지형 코드(01~10, 99) → 흙토람 공식 한글 이름 / 범례색.
  static QString terrainName(const QString& code);
  static QColor terrainColor(const QString& code);

  // soil_type_geo 필드 기준으로 공식 분포지형 색을 입힌다(폴리곤 레이어).
  static bool applyTerrainStyle(QgsVectorLayer* layer);
  // 폴리곤 안에 한글 지형명(산악지·하성평탄지 …)을 흰 테두리로 쓴다.
  static bool applyTerrainLabels(QgsVectorLayer* layer, double minAreaM2 = 40000.0,
                                 bool candidatesOnly = false);
  static QString terrainLabelExpression(double minAreaM2 = 0.0, bool candidatesOnly = false);

  // WFS 2.0 GetFeature. extent는 EPSG:4326 (lon/lat). hitsOnly면 numberMatched만.
  static QString wfsGetFeatureUrl(int tableNo, const QgsRectangle& extent4326,
                                  bool hitsOnly = false);

  // 흙토람 웹과 같은 분포지형 그림(ArcGIS 캐시, EPSG:3857). WMS crs에 5186을 넣지 않는다.
  static QString terrainPictureUri();
  // QGIS XYZ는 십진 {z}/{y}/{x}를 넣는다. 서버는 L00/Rxxxxxxxx/Cxxxxxxxx.png 만 받는다.
  static QUrl rewriteArcGisCacheUrl(const QUrl& url);

  static double maxSpanMeters() { return 80000.0; }
};
