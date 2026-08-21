#pragma once

#include <QColor>
#include <QHash>
#include <QString>

class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// KIGAM(한국지질자원연구원) 지오빅데이터 공개 GeoServer에서 1:5만 국토기본지질도
// 암상 폴리곤을 WFS로 내려받아 로컬 GPKG로 저장하고, 보고서 지질도 관례대로
// 지질단위(기호+지층명)별로 칠한 뒤 기호(Qa, PCEpgn 등)를 라벨로 표시한다.
// 단위색은 공식 지질도 래스터(WMS)에서 단위 내부점 픽셀을 샘플링해 도폭색
// 그대로 쓰고, 실패하면 지질시대(ICS) 계열색으로 대체한다.
//
//  - 서버: https://data.kigam.re.kr/geoserver (키·신청 불필요, EPSG:5186 재투영 지원)
//  - 레이어: geoOpen:l_50k_geology_litho_latest (전국 72,000여 폴리곤)
//  - 속성: 시대·지층·대표암상·기호·도폭명 — 시대를 정규화해 era_class 필드로 저장
//  - 한 번 내려받으면 GPKG로 남아 오프라인 현장에서도 쓸 수 있다.
class GeologyMapService {
public:
  // extent(EPSG:5186) 범위의 암상 폴리곤을 내려받아 outGpkgPath에 저장하고
  // 프로젝트 「참조 지도」 그룹에 추가한다. 성공 시 추가된 레이어를 반환.
  static QgsVectorLayer* downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                        const QgsRectangle& extent5186,
                                        const QString& outGpkgPath,
                                        QString* errorOut = nullptr);

  // 서버의 시대 문자열(예: "현생누대 신생대 제4기") → 정규화된 시대 분류 이름.
  static QString eraClass(const QString& eraText);
  // 정규화된 시대 분류 이름 → ICS 표준 지질시대색(대체색 계열의 기준).
  static QColor eraColor(const QString& eraClassName);

  // 데이터에 실제로 있는 지질단위(기호)만 골라 「기호 · 지층명」 범례를 만들고
  // 기호 라벨을 입힌다. officialColors(기호→도폭색)가 있으면 그 색을 쓴다.
  static bool applyGeologyStyle(QgsVectorLayer* layer,
                                const QHash<QString, QColor>& officialColors = {});

  static double maxSpanMeters() { return 20000.0; }
};
