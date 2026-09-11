#pragma once

#include <QString>
#include <QUrl>

#include "HeritageStyle.h"

// 국가유산 GIS통합인트라넷 자동화의 단계와 각 단계에서 실행할 스크립트.
//
// UI 없이 만들어 두어 검사할 수 있게 한다. 창(QWebEngineView)은 이 단계를 순서대로 돌리기만 한다.
//
// **확인한 것과 확인하지 못한 것을 구분한다.** 스크립트마다 verified 표시가 있고,
// 확인하지 못한 단계는 화면을 실제로 보고 고쳐야 한다. 추측한 선택자를 성공으로 처리하지 않는다.
enum class HeritageStage {
  Idle,
  Login,             // 확인함 (2026-09-11)
  DismissTutorial,   // 미확인 — 매번 뜨는 튜토리얼 안내
  OpenDownloadPage,  // 미확인 — 데이터 개방 › 국가유산자료 다운로드
  AgreeTerms,        // 미확인 — 원본자료 사용 서약서
  SelectDataset,     // 미확인 — 탭 6종
  SelectRegion,      // 미확인 — 시/도 → 시/군/구
  Search,            // 미확인
  CollectResults,    // 미확인 — 여러 페이지일 수 있다
  Download,          // 미확인
  Done,
  Failed,
};

class HeritageIntranetFlow {
public:
  static QUrl portalUrl();
  // 로그인 폼이 든 프레임의 주소. 루트는 frameset(top/main) 이라 프레임을 지정해야 하는데,
  // 이 주소로 바로 들어가면 프레임 없이 같은 폼을 만난다. 자동화는 이쪽을 쓴다.
  static QUrl loginUrl();

  static QString stageName(HeritageStage stage);
  // 이 단계의 스크립트가 실제 화면으로 확인된 것인가.
  static bool isVerified(HeritageStage stage);

  // --- 확인된 단계 ---

  // 로그인. 아이디/비밀번호를 칸에 넣고 페이지 자신의 goLogin() 을 부른다.
  //
  // 중요: 이 사이트는 **브라우저에서 RSA 로 암호화**한 뒤 숨은 칸(USER_ID/USER_PW)에 넣어
  // /j_spring_security_check 로 보낸다. 평문을 그대로 POST 하면 로그인되지 않는다.
  // 그래서 우리가 암호화하지 않고 페이지의 goLogin() 에 맡긴다.
  //
  // 반환 스크립트의 결과 문자열: "submitted" | "no-form" | "no-goLogin"
  // **이 스크립트에는 비밀번호가 들어 있다. 로그·화면·파일에 남기지 않는다.**
  static QString loginScript(const QString& userId, const QString& password);
  // 로그인 결과 확인용. 아직 로그인 폼이 보이면 실패다.
  static QString loginProbeScript();
  // 이 사이트의 루트는 frameset(top/main) 이다. runJavaScript 는 최상위 프레임에서만 돌아
  // frameset 위에서는 아무 요소도 찾지 못한다. 안쪽 프레임 주소를 알아내 그 주소로 직접 들어간다.
  // 반환: 안쪽 프레임의 URL, 또는 frameset 이 아니면 빈 문자열.
  static QString frameEscapeScript();

  // --- 미확인 단계 (실제 화면을 보고 채운다) ---
  //
  // 각 스크립트는 성공 시 그 뜻을 담은 문자열을, 못 찾으면 "not-found" 를 돌려준다.
  // 못 찾았는데 다음 단계로 넘어가지 않는다.

  static QString dismissTutorialScript();
  static QString openDownloadPageScript();
  // 다운로드 화면(또는 그 앞의 서약서 팝업)에 도착했는가. 눌렀다고 도착한 것으로 치지 않는다.
  // 반환: "ready" | "not-yet"
  static QString downloadPageProbeScript();
  static QString agreeTermsScript();
  static QString selectDatasetScript(HeritageDataset dataset);
  static QString selectRegionScript(const QString& sido, const QString& city);
  static QString searchScript();
  // 이 프레임이 다운로드 폼인가. wrap() 없이 각 QWebEngineFrame 에서 돌린다.
  // 반환: "codedeta" | "other" | "error"
  static QString formHintScript();
  // 「10개씩 보기」를 가장 큰 값으로. 쪽 수를 줄인다.
  static QString maximizePageSizeScript();
  // 현재 페이지 번호·전체 페이지 수·행 수를 읽는다. 결과가 여러 페이지일 수 있다.
  static QString resultPageInfoScript();

  // 결과를 전부 받는다. 「전체다운로드」가 있으면 그것을, 없으면 모든 행을 체크하고 「선택다운로드」를 누른다.
  // 반환: "all" | "selected" | "not-found"
  static QString downloadAllScript();
  // 한 페이지씩 받아야 할 때 다음 쪽으로 넘긴다. 반환: "moved" | "last-page" | "not-found"
  static QString goToPageScript(int pageNumber);

  // 지금 화면에 무엇이 있는지 요약해 돌려준다(JSON).
  // 미확인 단계에서 멈췄을 때, 다음에 어떤 선택자를 써야 하는지 사람이 보고 정하게 하려는 것이다.
  // 추측으로 채우지 않기 위한 장치다. 값(입력 내용)은 담지 않는다.
  static QString pageOutlineScript();

  // JS 문자열 리터럴로 안전하게 감싼다. 따옴표·역슬래시·줄바꿈·유니코드를 모두 처리한다.
  static QString jsString(const QString& value);

private:
  friend class HeritageIntranetFlowTest;
};
