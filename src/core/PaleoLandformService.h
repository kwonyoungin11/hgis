#pragma once

#include <QString>
#include <QStringList>

class QgsMapCanvas;
class QgsProject;
class QgsVectorLayer;

// 고지형분석 1단계: 흙토람 분포지형에서 충적 입지 후보를 강조하고,
// 구하도·자연제방 등 가설 판독 폴리곤을 「참조 지도」에 둔다.
// 조사 도메인(feature_poly)이 아니고 제출 SHP(5179)에도 넣지 않는다.
class PaleoLandformService {
public:
  static constexpr const char* kLayerKey = "paleo_landform";
  static constexpr const char* kLayerTitle = "고지형 판독";

  static bool isCandidateTerrainCode(const QString& code);
  static QStringList candidateTerrainCodes();
  static QStringList interpretationKinds();

  // 04 곡간/선상 · 05 해성평탄 · 06 하성평탄 · 08 홍적대지 강조, 나머지 흐림.
  static bool applyCandidateEmphasis(QgsVectorLayer* soilLayer);

  static QgsVectorLayer* findSoilTerrainLayer(QgsProject* project);

  // 04 선상지 · 05 해성평탄 · 08 하안단구. 06은 빈 값(하성평탄은 분할).
  static QString suggestKindFromTerrain(const QString& code);

  struct SeedResult {
    int added = 0;
    int replaced = 0;
    int keptUser = 0;
  };
  // 흙토람 04/05/06/08을 고지형 판독 가설로 깐다. note가 "자동:"인 것만 교체.
  static SeedResult seedInterpretationFromSoil(QgsVectorLayer* soilLayer, QgsVectorLayer* paleoLayer,
                                               QString* errorOut = nullptr);

  // 사용자 클릭으로만 범례에 올린다. 빈 도메인 자동 추가 없음.
  static QgsVectorLayer* ensureInterpretationLayer(QgsProject* project, const QString& gpkgPath,
                                                   QString* errorOut = nullptr);

  static bool applyInterpretationStyle(QgsVectorLayer* layer);
};
