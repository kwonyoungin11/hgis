#pragma once

#include <QDateTime>
#include <QDialog>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/HeritageIntranetFlow.h"
#include "core/HeritageStyle.h"

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTimer;
class QWebEngineView;
class QTabWidget;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineDownloadRequest;

// 국가유산 GIS통합인트라넷 자동화 창.
//
// 원칙 (수치지형도 창과 같다):
// - 공식 화면은 숨기고 단계·받은 파일·실패 사유만 보여 준다
// - **실패를 성공으로 처리하지 않는다.** 스크립트가 요소를 못 찾으면 거기서 멈추고,
//   그 화면에 무엇이 있었는지(pageOutline)를 함께 보여 준다. 다음 선택자는 그걸 보고 정한다
// - 서약서 동의는 자동으로 하되 **동의 시각과 서약서 원문을 영수증으로 남긴다**
// - **시/군까지만** 고른다
// - 비밀번호는 화면·로그·진단문에 절대 담지 않는다
class KaHeritageBrowser : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageBrowser(QWidget* parent = nullptr);
  ~KaHeritageBrowser() override;

  void setDownloadRoot(const QString& directory);
  // 받을 대상. 시/군 하나와 자료 종류들.
  void setTarget(const QString& sido, const QString& city,
                 const QVector<HeritageDataset>& datasets);
  // 아직 받기를 시작하지 않았을 때 창에 상태만 보여 준다(시·군 판정 중 등).
  void showWaiting(const QString& message);
  void start();
  void stop();

  HeritageStage stage() const { return m_stage; }
  // 멈춘 자리에서 화면에 무엇이 있었는지. 선택자를 정하는 데 쓴다. 비밀번호는 들어가지 않는다.
  QString lastOutline() const { return m_lastOutline; }

signals:
  void stageChanged(HeritageStage stage, const QString& message);
  // 한 종류를 다 받았다. 호출자가 HeritageImport 로 지도에 올린다.
  void datasetReady(HeritageDataset dataset, const QStringList& files);
  // 여섯 종을 모두 마쳤다.
  void allFinished();
  void failed(const QString& message);
  void fileDownloaded(const QString& path);
  // 서약서에 동의한 시각과 그때 화면에 있던 원문. 호출자가 receipts/ 에 남긴다.
  void agreementAccepted(const QDateTime& when, const QString& termsText);

private:
  void setStage(HeritageStage stage, const QString& message);
  void fail(const QString& message);
  void runStage();
  void runScript(const QString& script, const std::function<void(const QString&)>& then,
                 bool requireForm = false);
  // 다운로드 폼(codedeta) 프레임이 있으면 거기서 돌린다.
  // requireForm 이면 폼이 없을 때 메인(지도)에 검색을 넣지 않는다.
  void runOnPreferredDocument(const QString& script,
                              const std::function<void(const QString&)>& then,
                              bool manageFlight, bool requireForm);
  void captureOutline(const std::function<void()>& then);
  void handleDownload(QWebEngineDownloadRequest* request);
  HeritageStage nextStage(HeritageStage stage) const;
  // 팝업까지 포함해 새 창을 탭으로 받는다. 이것이 없으면 서약서·다운로드 팝업이 통째로 버려진다.
  QWebEngineView* addPage(QWebEnginePage* opener = nullptr);
  // 지금 스크립트를 돌릴 페이지. 인트라넷 페이지 중 가장 최근에 뜬 것.
  QWebEnginePage* activePage() const;

  QWebEngineProfile* m_profile = nullptr;
  QTabWidget* m_tabs = nullptr;
  QLabel* m_stageLabel = nullptr;
  QLabel* m_detailLabel = nullptr;
  QPlainTextEdit* m_outline = nullptr;
  QPushButton* m_stopButton = nullptr;
  QTimer* m_poll = nullptr;

  HeritageStage m_stage = HeritageStage::Idle;
  QString m_sido;
  QString m_city;
  QVector<HeritageDataset> m_datasets;
  int m_datasetIndex = 0;
  QString m_downloadRoot;
  QString m_lastOutline;
  QString m_lastAlert;  // 사이트가 마지막으로 띄운 알림 글
  int m_waitTicks = 0;
  // 받는 중인 파일 수. 0이 되고 조금 더 조용하면 그 종류를 마친 것으로 본다.
  int m_pendingDownloads = 0;
  int m_idleTicks = 0;
  QStringList m_currentFiles;
  QString m_downloadMode;  // "all" = 결과 전체 한 번에, "selected" = 쪽마다
  int m_currentPage = 1;
  int m_lastPage = 1;
  int m_totalBeforeSearch = -1;  // 조건을 걸기 전 검색결과 건수(전국)
  bool m_running = false;
  bool m_scriptInFlight = false;
  bool m_pageReady = false;  // loadFinished 전에는 스크립트를 돌리지 않는다
  bool m_frameChecked = false;  // 이 화면에서 프레임 탈출을 이미 봤는가
  bool m_openedFrameSrc = false;  // iframe src 를 탭으로 연 적
  QString m_lastCppProbe;         // 프레임 url/name/hint. 비밀번호 없음
  quint64 m_generation = 0;
};
