#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

// 국가유산 인트라넷 다운로드 화면의 **폼을 HTML 에서 읽어 낸다.**
//
// 왜 이렇게 하나:
// 브라우저 화면을 JS 로 흉내 내 누르는 방식은 프레임·ajax·사이트 개편에 매번 깨졌다.
// 폼의 주소와 칸 이름만 알면 HTTP 요청 하나로 끝난다 — 프레임도 클릭도 필요 없다.
//
// 칸 이름을 **지어내지 않는다.** 실제 HTML 에 있는 것만 쓴다.
struct HeritageForm {
  QString action;                       // 보낼 주소 (비어 있으면 받아 온 주소 그대로)
  QString method = QStringLiteral("POST");
  QMap<QString, QString> fields;        // name → 현재 값 (hidden 포함, 그대로 되돌려 보낸다)
  QString sidoField;                    // 시/도 select 의 name
  QString sigunguField;                 // 시/군/구 select 의 name
  QMap<QString, QString> sidoOptions;   // 보이는 글자 → 값
  QMap<QString, QString> sigunguOptions;
  QStringList warnings;

  bool ok() const { return !sidoField.isEmpty() && !fields.isEmpty(); }
};

class HeritageFormParser {
public:
  // anchor 가 id/name 에 들어간 select 를 품은 form 을 고른다.
  // 지도 패널의 bjdcd* 폼과 헷갈리지 않게 하는 장치다.
  static HeritageForm parse(const QByteArray& html,
                            const QString& anchor = QStringLiteral("codedeta"));

  // 보이는 글자로 옵션 값을 찾는다. 「강원도」와 「강원특별자치도」,
  // 「전라남도」와 「전남광주통합특별시」처럼 표기가 달라도 맞춘다.
  static QString matchOption(const QMap<QString, QString>& options, const QString& wanted);

  // 검색결과 「N건」을 읽는다. 없으면 -1.
  static int resultCount(const QByteArray& html);
};
