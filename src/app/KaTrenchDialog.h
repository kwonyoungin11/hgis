#pragma once

#include <QByteArray>
#include <QDialog>

#include "core/TrenchGridGenerator.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

// 시굴격자 속성 창(모덜리스). 규격·둑 간격·행/열·회전(방위)·이름 접두를 세밀하게
// 조정하고, 조사구역 자동 채움일 때 시굴 비율(기준: 시굴 10% · 표본 2%)을 미리 보여준다.
// 「적용」은 기존 격자를 대체한다. 개별 트렌치 삭제는 이동 도구에서 우클릭.
class KaTrenchDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaTrenchDialog(QWidget* parent = nullptr);

  // 선택한(없으면 마지막) survey_area WKB와 면적(㎡). 비어 있으면 수동 배치만.
  void setArea(const QByteArray& wkb, double areaM2);
  TrenchGridGenerator::Spec spec() const;
  bool autoFill() const;
  QByteArray areaWkb() const { return m_areaWkb; }
  double areaM2() const { return m_areaM2; }

signals:
  void applyRequested();        // 현재 설정으로 재배치(기존 격자 대체)
  void manualPlaceRequested();  // 맵에서 원점 클릭 배치
  void editSingleRequested();   // 개별 편집(그래픽처럼 선택·이동·삭제)
  void moveRequested();         // 전체 이동 도구 켜기

private:
  void refreshPlan();

  QCheckBox* m_auto = nullptr;
  QComboBox* m_size = nullptr;
  QDoubleSpinBox* m_balk = nullptr;
  QSpinBox* m_rows = nullptr;
  QSpinBox* m_cols = nullptr;
  QDoubleSpinBox* m_az = nullptr;
  QLineEdit* m_prefix = nullptr;
  QLabel* m_ratio = nullptr;
  QByteArray m_areaWkb;
  double m_areaM2 = 0.0;
};
