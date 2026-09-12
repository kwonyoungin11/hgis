#pragma once

#include <QByteArray>
#include <QNetworkCookie>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include "HeritageFormParser.h"

class QNetworkAccessManager;
class QNetworkReply;

// 국가유산 인트라넷을 **HTTP 로 직접** 다룬다.
//
// 왜 이렇게 하나:
// 브라우저 화면을 JS 로 흉내 내 누르는 방식은 프레임·ajax·사이트 개편에 매번 깨졌다.
// 로그인만 브라우저로 하고(비밀번호를 RSA 로 암호화하므로), 그 뒤는 세션 쿠키로 요청한다.
// 프레임도 클릭도 없다. 실패하면 어디서 왜 실패했는지 남는다.
class HeritageHttpClient : public QObject {
  Q_OBJECT
public:
  explicit HeritageHttpClient(QObject* parent = nullptr);
  ~HeritageHttpClient() override;

  // 브라우저에서 로그인한 세션 쿠키. 이게 없으면 아무것도 못 받는다.
  void setCookies(const QList<QNetworkCookie>& cookies);
  bool hasSession() const;

  // 보낼 몸통을 만든다. 폼의 칸을 그대로 되돌려 보내고 시/도·시/군/구만 바꾼다.
  // 칸 이름을 지어내지 않는다.
  static QByteArray buildBody(const HeritageForm& form, const QString& sidoValue,
                              const QString& sigunguValue,
                              const QMap<QString, QString>& extra = {});

  // 폼의 action 과 받아 온 주소로 실제 보낼 주소를 정한다.
  static QUrl resolveAction(const QUrl& pageUrl, const HeritageForm& form);

  // ① 다운로드 화면을 받아 폼을 읽는다.
  // 응답을 그대로 남길 폴더. 폼을 못 읽으면 받은 HTML 을 여기에 저장한다.
  // 추측하지 않고 실제로 무엇이 왔는지 보기 위한 것이다.
  void setDumpDir(const QString& dir);

  void fetchDownloadPage();
  // ② 시/군으로 좁혀 검색한다. 건수가 줄지 않으면 실패로 본다(전국을 받지 않는다).
  void search(const QString& sido, const QString& city);

signals:
  void pageReady(const HeritageForm& form, int totalBeforeSearch);
  void searchReady(int count, const QByteArray& html);
  void failed(const QString& message);

private:
  QNetworkReply* post(const QUrl& url, const QByteArray& body);

  QNetworkAccessManager* m_nam = nullptr;
  HeritageForm m_form;
  QUrl m_pageUrl;
  int m_totalBeforeSearch = -1;
  bool m_hasSession = false;
  QString m_dumpDir;

  // 받은 HTML 을 파일로 남기고 그 경로를 돌려준다.
  QString dump(const QString& tag, const QByteArray& html) const;
};
