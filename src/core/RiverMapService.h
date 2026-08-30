#pragma once

#include <QString>

class QgsMapCanvas;
class QgsProject;
class QgsRectangle;
class QgsVectorLayer;

// VWorld 공개 WFS에서 하천망(국가·지방1급·지방2급 하천구역 폴리곤)을 내려받아
// 로컬 GPKG로 저장하고, 보고서 수계도 관례대로 하천 등급별 물색으로 칠한 뒤
// 하천명(riv_nm)을 파란 라벨로 표시한다.
//
//  - 서버: https://api.vworld.kr/req/wfs (VWorld 인증키 필요 — 배경지도와 같은 키)
//  - 레이어: lt_c_wkmstrm (하천망도, WAMIS 유래) — riv_nm·riv_level 속성
//  - 응답은 EPSG:4326으로 받아 EPSG:5186으로 재투영해 저장한다.
//  - 한 번 내려받으면 GPKG로 남아 오프라인 현장에서도 쓸 수 있다.
class RiverMapService {
public:
  // extent(EPSG:5186) 범위의 하천망을 내려받아 outGpkgPath에 저장하고
  // 프로젝트 「참조 지도」 그룹에 추가한다. 성공 시 추가된 레이어를 반환.
  static QgsVectorLayer* downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                        const QgsRectangle& extent5186,
                                        const QString& apiKey, const QString& outGpkgPath,
                                        QString* errorOut = nullptr);

  // 하천 등급(국가하천/지방1급/지방2급)별 물색 범례 + 하천명 라벨을 입힌다.
  static bool applyRiverStyle(QgsVectorLayer* layer);

  // 수계도는 유역 맥락이 필요해 지질도보다 넓게 허용한다(기존 40 km의 4배).
  static double maxSpanMeters() { return 160000.0; }
};
