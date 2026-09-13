#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "HeritageStyle.h"

class QgsProject;
class QgsVectorLayer;

// 인트라넷에서 받은 국가유산 자료를 지도에 올린다.
//
// 수치지형도와 정반대다. 수치지형도는 밑그림이라 한 레이어로 합치고 속성을 버렸지만,
// 주변유적은 **유적명과 정보가 붙어 있어야** 클릭해 읽고 보고서에 쓴다.
// 자료 종류별로 레이어를 나누고, 종류마다 고정 색과 유적명 범례를 건다.
//
// 받은 자료는 조사폴더 안에만 둔다. 포터블·제출물에 실리지 않는다.
namespace HeritageImport {

struct Result {
  QList<QgsVectorLayer*> layers;
  int featureCount = 0;
  QStringList messages;  // 사용자에게 보여 줄 알림(상한 초과, 필드 못 찾음 등)
  QString error;
  bool ok() const { return error.isEmpty() && !layers.isEmpty(); }
};

// 실제 필드 목록에서 유적명 컬럼을 고른다.
// 이름을 추측해 박지 않는다 — 있는 것 중에서 고르고, 없으면 빈 문자열을 돌려준다.
QString chooseNameField(const QgsVectorLayer* layer);

// 받은 파일(ZIP 또는 SHP 세트)을 풀어 한 종류의 레이어로 올린다.
// archiveRoot 아래에 원본을 보관하고, 그 사본을 연다. 원본은 바꾸지 않는다.
Result loadDataset(QgsProject* project, HeritageDataset dataset,
                   const QStringList& downloadedFiles, const QString& archiveRoot);

// 참조 지도 그룹 이름. 조사 데이터와 섞이지 않게 여기에만 둔다.
QString referenceGroupName();

}  // namespace HeritageImport
