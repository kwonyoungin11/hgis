#pragma once

#include <QColor>
#include <QString>

class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// 흙토람(농촌진흥청 국립농업과학원) 공개 GeoServer에서 정밀토양도 폴리곤을
// WFS로 내려받아 로컬 GPKG로 저장하고, 분포지형(soil_type_geo)을 흙토람 공식
// 범례색으로 표시한다.
//
//  - 서버: https://gis.naas.go.kr/geoserver/soilmap (키·신청 불필요, EPSG:5186 네이티브)
//  - 테이블 SOIL_1·SOIL_2·SOIL_3은 지역 분할이므로 병합해도 중복이 없다.
//  - 한 번 내려받으면 GPKG로 남아 오프라인 현장에서도 쓸 수 있다.
class SoilMapService {
public:
  // extent(EPSG:5186) 범위의 토양 폴리곤을 내려받아 outGpkgPath에 저장하고
  // 프로젝트 「참조 지도」 그룹에 추가한다. 성공 시 추가된 레이어를 반환.
  // 넓은 범위(한 변 maxSpanMeters 초과)는 서버 부담을 피하려 거부한다.
  static QgsVectorLayer* downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                        const QgsRectangle& extent5186,
                                        const QString& outGpkgPath,
                                        QString* errorOut = nullptr);

  // 분포지형 코드(01~10, 99) → 흙토람 공식 한글 이름 / 범례색.
  static QString terrainName(const QString& code);
  static QColor terrainColor(const QString& code);

  // soil_type_geo 필드 기준으로 공식 분포지형 색을 입힌다(폴리곤 레이어).
  static bool applyTerrainStyle(QgsVectorLayer* layer);

  static double maxSpanMeters() { return 20000.0; }
};
