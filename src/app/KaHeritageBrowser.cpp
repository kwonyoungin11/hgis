#include "KaHeritageBrowser.h"

#include "core/HeritageFormParser.h"
#include "core/HeritageIntranetFlow.h"

#include "core/HeritageIntranetSettings.h"

#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QLabel>
#include <QMutex>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFrame>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <memory>
#include <optional>

// 사이트가 실제로 보내는 요청을 그대로 적는다.
//
// 프레임 안 DOM 을 찾아 누르는 방식은 화면 구조가 조금만 달라도 통째로 멈춘다.
// 그래서 「무엇을 찾을까」가 아니라 「무엇을 보냈나」를 기록한다.
// 여기서 나온 주소·파라미터를 알면 브라우저 없이 HTTP 로 바로 받을 수 있다.
//
// 값은 남기되 **보낸 본문은 적지 않는다** — 로그인 요청에 비밀번호가 들어가기 때문이다.
class HeritageRequestLog : public QWebEngineUrlRequestInterceptor {
public:
  explicit HeritageRequestLog(QObject* parent = nullptr)
      : QWebEngineUrlRequestInterceptor(parent) {}

  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    // 무엇을 남기고 무엇을 지울지는 한 곳에서 정한다(검사가 그 규칙을 붙잡는다).
    const auto type = info.resourceType();
    if (type == QWebEngineUrlRequestInfo::ResourceTypeImage ||
        type == QWebEngineUrlRequestInfo::ResourceTypeFontResource ||
        type == QWebEngineUrlRequestInfo::ResourceTypeStylesheet ||
        type == QWebEngineUrlRequestInfo::ResourceTypeScript)
      return;
    const QString line = HeritageIntranetFlow::redactRequestLine(
        QString::fromLatin1(info.requestMethod()), info.requestUrl());

    QMutexLocker lock(&m_mutex);
    if (!m_lines.isEmpty() && m_lines.last() == line) return;  // 같은 요청 반복은 한 줄로
    m_lines.append(line);
    while (m_lines.size() > 60) m_lines.removeFirst();
  }

  QStringList lines() const {
    QMutexLocker lock(&m_mutex);
    return m_lines;
  }

  void clear() {
    QMutexLocker lock(&m_mutex);
    m_lines.clear();
  }

private:
  mutable QMutex m_mutex;  // 가로채기는 다른 스레드에서 불린다
  QStringList m_lines;
};

namespace {
constexpr int kPollMs = 700;

// 이 사이트는 window.open 으로 창을 연다(공지·서약서·다운로드).
// createWindow 를 구현하지 않으면 그 창이 통째로 버려진다 — 수치지형도 창과 같은 구조다.
class HeritagePage : public QWebEnginePage {
public:
  HeritagePage(QWebEngineProfile* profile, QObject* parent,
               std::function<QWebEngineView*(QWebEnginePage*)> spawn,
               std::function<void(const QString&)> note)
      : QWebEnginePage(profile, parent), m_spawn(std::move(spawn)), m_note(std::move(note)) {}

  QWebEnginePage* createWindow(WebWindowType) override {
    QWebEngineView* view = m_spawn ? m_spawn(this) : nullptr;
    return view ? view->page() : nullptr;
  }

  // 내려받기를 누르면 좌표체계 안내(EPSG:5179) 알림이 뜬다(2026-09-11 확인).
  // 아무도 「확인」을 누르지 않으면 거기서 멈춘다. 자동으로 닫되 무슨 말이었는지는 남긴다.
  void javaScriptAlert(const QUrl&, const QString& message) override {
    if (m_note) m_note(message);
  }
  bool javaScriptConfirm(const QUrl&, const QString& message) override {
    if (m_note) m_note(message);
    return true;  // 받기를 계속한다
  }
  bool javaScriptPrompt(const QUrl&, const QString& message, const QString&, QString*) override {
    // 값을 지어내지 않는다. 물어보는 것이 있으면 그렇다고 알리고 취소한다.
    if (m_note) m_note(message);
    return false;
  }

private:
  std::function<QWebEngineView*(QWebEnginePage*)> m_spawn;
  std::function<void(const QString&)> m_note;
};

constexpr int kMaxWaitTicks = 75;  // 수치지형도 창과 같은 여유(약 52초)

void collectFrames(const QWebEngineFrame& frame, QVector<QWebEngineFrame>* out) {
  if (!frame.isValid()) return;
  out->push_back(frame);
  const QList<QWebEngineFrame> kids = frame.children();
  for (const QWebEngineFrame& child : kids)
    collectFrames(child, out);
}

QVector<QWebEngineFrame> allFrames(QTabWidget* tabs) {
  QVector<QWebEngineFrame> frames;
  if (!tabs) return frames;
  for (int i = 0; i < tabs->count(); ++i) {
    auto* view = qobject_cast<QWebEngineView*>(tabs->widget(i));
    if (!view || !view->page()) continue;
    collectFrames(view->page()->mainFrame(), &frames);
  }
  return frames;
}

QVector<QWebEnginePage*> allPages(QTabWidget* tabs) {
  QVector<QWebEnginePage*> pages;
  if (!tabs) return pages;
  for (int i = 0; i < tabs->count(); ++i) {
    auto* view = qobject_cast<QWebEngineView*>(tabs->widget(i));
    if (view && view->page()) pages.push_back(view->page());
  }
  return pages;
}

void appendUnique(QVector<QWebEngineFrame>* frames, const QWebEngineFrame& frame) {
  if (!frames || !frame.isValid()) return;
  if (frames->contains(frame)) return;
  frames->push_back(frame);
}

void mergeNamedFrames(QWebEnginePage* page, const QJsonArray& list, QVector<QWebEngineFrame>* frames) {
  if (!page || !frames) return;
  for (const QJsonValue& value : list) {
    const QJsonObject row = value.toObject();
    const QStringList keys = {row.value(QStringLiteral("name")).toString(),
                              row.value(QStringLiteral("id")).toString()};
    for (const QString& key : keys) {
      if (key.trimmed().isEmpty()) continue;
      const std::optional<QWebEngineFrame> found = page->findFrameByName(key);
      if (found && found->isValid()) appendUnique(frames, *found);
    }
  }
}

bool isHeritageHost(const QUrl& url) {
  return url.host().endsWith(QStringLiteral("gis-heritage.go.kr"));
}

QString safeName(const QString& suggested) {
  QString name = QFileInfo(suggested).fileName().trimmed();
  name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
  return name.isEmpty() ? QStringLiteral("download.bin") : name;
}
}  // namespace

KaHeritageBrowser::KaHeritageBrowser(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("주변유적 받기"));
  setModal(false);
  resize(980, 720);

  auto* root = new QVBoxLayout(this);
  m_stageLabel = new QLabel(QStringLiteral("대기"), this);
  m_stageLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
  m_detailLabel = new QLabel(QString(), this);
  m_detailLabel->setWordWrap(true);
  root->addWidget(m_stageLabel);
  root->addWidget(m_detailLabel);

  m_profile = new QWebEngineProfile(QStringLiteral("ka-heritage"), this);
  m_requestLog = new HeritageRequestLog(this);
  m_profile->setUrlRequestInterceptor(m_requestLog);

  // 로그인 세션 쿠키를 모은다. 이걸로 검색·다운로드를 HTTP 로 직접 한다.
  connect(m_profile->cookieStore(), &QWebEngineCookieStore::cookieAdded, this,
          [this](const QNetworkCookie& cookie) {
            m_cookies.removeIf([&cookie](const QNetworkCookie& c) { return c.name() == cookie.name(); });
            m_cookies.append(cookie);
          });

  m_http = new HeritageHttpClient(this);
  connect(m_http, &HeritageHttpClient::failed, this, [this](const QString& why) {
    fail(QStringLiteral("HTTP 경로: %1").arg(why));
  });
  connect(m_http, &HeritageHttpClient::pageReady, this,
          [this](const HeritageForm& form, int total) {
            m_totalBeforeSearch = total;
            m_detailLabel->setText(
                QStringLiteral("검색 폼을 읽었습니다(시·도 %1, 시·군·구 %2, 조건 전 %3건). %4 %5 검색합니다.")
                    .arg(form.sidoField, form.sigunguField)
                    .arg(total)
                    .arg(m_sido, m_city));
            m_http->search(m_sido, m_city);
          });
  connect(m_http, &HeritageHttpClient::searchReady, this,
          [this](int count, const QByteArray&) {
            setStage(HeritageStage::Download,
                     QStringLiteral("%1 %2 — 검색결과 %3건. 내려받습니다.")
                         .arg(m_sido, m_city)
                         .arg(count));
          });
  m_tabs = new QTabWidget(this);
  m_tabs->setDocumentMode(true);
  // 공식 사이트 화면은 숨긴다(수치지형도 창과 같은 원칙). 단계·진행만 보여 준다.
  // 막혔을 때만 「자세히」로 펼쳐 본다.
  m_tabs->setVisible(false);
  root->addWidget(m_tabs, 1);

  // 아직 확인하지 못한 단계에서 멈췄을 때, 그 화면에 무엇이 있었는지 보여 준다.
  m_outline = new QPlainTextEdit(this);
  m_outline->setReadOnly(true);
  m_outline->setMaximumHeight(140);
  m_outline->setPlaceholderText(
      QStringLiteral("멈춘 자리에서 화면에 있던 것들이 여기에 나옵니다. 다음 선택자를 정할 때 씁니다."));
  root->addWidget(m_outline);

  auto* buttons = new QHBoxLayout;
  buttons->addStretch(1);
  auto* detailsButton = new QPushButton(QStringLiteral("자세히"), this);
  detailsButton->setCheckable(true);
  detailsButton->setToolTip(QStringLiteral("공식 사이트 화면을 펼쳐 봅니다. 막혔을 때만 쓰면 됩니다."));
  connect(detailsButton, &QPushButton::toggled, this, [this](bool on) {
    if (m_tabs) m_tabs->setVisible(on);
  });
  buttons->addWidget(detailsButton);
  m_stopButton = new QPushButton(QStringLiteral("중지"), this);
  connect(m_stopButton, &QPushButton::clicked, this, &KaHeritageBrowser::stop);
  buttons->addWidget(m_stopButton);
  root->addLayout(buttons);

  m_poll = new QTimer(this);
  m_poll->setInterval(kPollMs);
  connect(m_poll, &QTimer::timeout, this, &KaHeritageBrowser::runStage);

  connect(m_profile, &QWebEngineProfile::downloadRequested, this,
          &KaHeritageBrowser::handleDownload);
}

KaHeritageBrowser::~KaHeritageBrowser() { m_poll->stop(); }

void KaHeritageBrowser::setDownloadRoot(const QString& directory) {
  m_downloadRoot = directory;
}

void KaHeritageBrowser::setTarget(const QString& sido, const QString& city,
                                  const QVector<HeritageDataset>& datasets) {
  m_sido = sido.trimmed();
  m_city = city.trimmed();
  m_datasets = datasets;
  m_datasetIndex = 0;
}

void KaHeritageBrowser::logLine(const QString& text) {
  // 조사폴더가 없을 수도 있으니 리포의 고정 자리에도 함께 남긴다.
  QStringList targets;
  if (!m_downloadRoot.isEmpty()) {
    const QString dir =
        QDir(QDir(m_downloadRoot).absolutePath() + QStringLiteral("/../receipts")).absolutePath();
    if (QDir().mkpath(dir)) targets << QDir(dir).filePath(QStringLiteral("진단.txt"));
  }
  if (QDir().mkpath(QStringLiteral("build/qa")))
    targets << QStringLiteral("build/qa/heritage-debug.txt");

  const QString line = QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                           .arg(text);
  for (const QString& path : targets) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) continue;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << line << Qt::endl;
  }
}

void KaHeritageBrowser::setStage(HeritageStage stage, const QString& message) {
  m_stage = stage;
  m_waitTicks = 0;
  // 화면이 자리잡을 시간을 준다. ajax 로 폼을 다시 그리는 사이트라
  // 바로 다음 동작을 하면 빈 값을 잡는다(2026-09-12: 1초 만에 골라 전국을 요청했다).
  m_settle = 3;
  // 자료 종류를 바꾸면 폼 전체가 새로 그려지고 시/군 목록도 다시 채워진다. 더 기다린다.
  if (stage == HeritageStage::SelectRegion) m_settle = 6;
  logLine(QStringLiteral("단계 → %1 : %2")
              .arg(HeritageIntranetFlow::stageName(stage), message));
  m_stageLabel->setText(HeritageIntranetFlow::stageName(stage));
  m_detailLabel->setText(message);
  emit stageChanged(stage, message);
}

QString KaHeritageBrowser::saveRequestLog() {
  if (!m_requestLog || m_downloadRoot.isEmpty()) return {};
  const QStringList sent = m_requestLog->lines();
  if (sent.isEmpty()) return {};
  // 조사폴더/주변유적/receipts/ 에 남긴다. 배포 경로가 아니다.
  const QDir root(m_downloadRoot);
  const QString dir = QDir(root.absolutePath() + QStringLiteral("/../receipts")).absolutePath();
  if (!QDir().mkpath(dir)) return {};
  const QString path =
      QDir(dir).filePath(QStringLiteral("requests-%1.txt")
                             .arg(QDateTime::currentDateTime().toString(
                                 QStringLiteral("yyyyMMdd-HHmmss"))));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QStringLiteral("국가유산 인트라넷이 보낸 요청 기록") << Qt::endl;
  out << QStringLiteral("기록 시각: ")
      << QDateTime::currentDateTime().toString(Qt::ISODate) << Qt::endl;
  out << QStringLiteral("로그인 요청의 내용은 남기지 않는다.") << Qt::endl;
  out << QStringLiteral("----") << Qt::endl;
  for (const QString& line : sent) out << line << Qt::endl;
  return path;
}

void KaHeritageBrowser::fail(const QString& message) {
  logLine(QStringLiteral("실패: %1").arg(message));
  m_running = false;
  m_poll->stop();
  m_stage = HeritageStage::Failed;
  m_stageLabel->setText(HeritageIntranetFlow::stageName(HeritageStage::Failed));
  m_detailLabel->setText(message);
  // 요청 기록을 파일로 남긴다. 화면에서 긁어 오지 않아도 되게.
  const QString logPath = saveRequestLog();
  if (!logPath.isEmpty())
    m_detailLabel->setText(QStringLiteral("%1\n요청 기록: %2").arg(message, logPath));
  // 멈춘 자리를 그대로 보여 준다. 무엇 때문에 못 갔는지 사람이 볼 수 있어야 한다.
  captureOutline([this, message]() {
    emit failed(message);
  });
}

void KaHeritageBrowser::stop() {
  if (!m_running) return;
  m_running = false;
  ++m_generation;
  m_scriptInFlight = false;
  m_poll->stop();
  setStage(HeritageStage::Idle, QStringLiteral("사용자가 중지했습니다."));
}

void KaHeritageBrowser::showWaiting(const QString& message) {
  m_stageLabel->setText(QStringLiteral("준비 중"));
  m_detailLabel->setText(message);
}

void KaHeritageBrowser::start() {
  const auto account = HeritageIntranetSettings::credentials();
  if (account.username.trimmed().isEmpty() || account.password.isEmpty()) {
    fail(QStringLiteral("국가유산 인트라넷 계정이 없습니다. 더보기의 계정 설정에서 넣어 주세요."));
    return;
  }
  if (m_sido.isEmpty() || m_city.isEmpty()) {
    fail(QStringLiteral("받을 시·군이 정해지지 않았습니다."));
    return;
  }
  if (m_downloadRoot.isEmpty()) {
    fail(QStringLiteral("저장할 폴더가 정해지지 않았습니다."));
    return;
  }

  ++m_generation;
  m_scriptInFlight = false;
  m_openedFrameSrc = false;
  m_lastCppProbe.clear();
  m_running = true;
  m_datasetIndex = 0;
  m_lastOutline.clear();
  m_httpStarted = false;
  m_mainView = nullptr;
  m_listRequested = false;
  m_listSent = false;
  m_agreementRecorded = false;
  m_form = HeritageForm();
  m_outline->clear();
  logLine(QStringLiteral("=== 시작: %1 %2 · 자료 %3종 ===").arg(m_sido, m_city).arg(m_datasets.size()));
  setStage(HeritageStage::Login,
           QStringLiteral("%1 로그인 중입니다.").arg(HeritageIntranetSettings::describeForLog()));
  while (m_tabs->count() > 0) {
    QWidget* w = m_tabs->widget(0);
    m_tabs->removeTab(0);
    w->deleteLater();
  }
  m_pageReady = false;
  QWebEngineView* view = addPage();
  view->load(HeritageIntranetFlow::loginUrl());
  m_poll->start();
}

void KaHeritageBrowser::runScript(const QString& script,
                                  const std::function<void(const QString&)>& then,
                                  bool requireForm) {
  if (m_scriptInFlight) return;
  m_scriptInFlight = true;
  runOnPreferredDocument(script, then, true, requireForm);
}

void KaHeritageBrowser::runOnPreferredDocument(const QString& script,
                                               const std::function<void(const QString&)>& then,
                                               bool manageFlight, bool requireForm) {
  const quint64 generation = m_generation;
  const QVector<QWebEnginePage*> pages = allPages(m_tabs);

  auto finishEmpty = [this, then, manageFlight]() {
    if (manageFlight) m_scriptInFlight = false;
    if (then) then(QStringLiteral("not-found"));
  };

  auto invoke = [this, generation, script, then, manageFlight](QWebEngineFrame frame) {
    auto done = [this, generation, then, manageFlight](const QVariant& value) {
      if (manageFlight) m_scriptInFlight = false;
      if (generation != m_generation) return;
      const QString result = value.toString();
      // 스크립트 **본문은 남기지 않는다**(로그인 스크립트에 비밀번호가 들어 있다). 결과만 남긴다.
      if (!result.isEmpty() && result.size() < 400)
        logLine(QStringLiteral("  [%1] 결과: %2")
                    .arg(HeritageIntranetFlow::stageName(m_stage), result));
      else if (!result.isEmpty())
        logLine(QStringLiteral("  [%1] 결과: %2바이트")
                    .arg(HeritageIntranetFlow::stageName(m_stage))
                    .arg(result.size()));
      if (then) then(result);
    };
    if (frame.isValid()) {
      frame.runJavaScript(script, done);
      return;
    }
    QWebEnginePage* page = activePage();
    if (!page) {
      if (manageFlight) m_scriptInFlight = false;
      if (then) then(QString());
      return;
    }
    page->runJavaScript(script, done);
  };

  auto openSrcs = [this](QWebEnginePage* page, const QJsonArray& list) -> int {
    if (m_openedFrameSrc || !page) return 0;
    int opened = 0;
    const QUrl base = page->url();
    for (const QJsonValue& value : list) {
      const QString src = value.toObject().value(QLatin1String("src")).toString().trimmed();
      if (src.isEmpty() || src.startsWith(QLatin1String("javascript:"), Qt::CaseInsensitive))
        continue;
      const QUrl url = base.resolved(QUrl(src));
      if (!url.isValid() || !isHeritageHost(url) || url == base) continue;
      bool have = false;
      for (int i = 0; i < m_tabs->count(); ++i) {
        auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
        if (view && view->url() == url) {
          have = true;
          break;
        }
      }
      if (have) continue;
      m_openedFrameSrc = true;
      m_pageReady = false;
      addPage()->load(url);
      ++opened;
      m_detailLabel->setText(QStringLiteral("다운로드 화면 프레임으로 들어갑니다…"));
    }
    return opened;
  };

  auto hintFrames = [this, generation, invoke, finishEmpty, openSrcs, manageFlight, requireForm,
                     then](QVector<QWebEngineFrame> frames, int jsFrames, QWebEnginePage* srcPage,
                           const QJsonArray& srcList) {
    QVector<QWebEngineFrame> valid;
    for (const QWebEngineFrame& frame : frames) {
      if (frame.isValid()) valid.push_back(frame);
    }
    const int n = valid.size();
    if (n == 0) {
      if (requireForm) {
        finishEmpty();
        return;
      }
      invoke(QWebEngineFrame());
      return;
    }
    auto pending = std::make_shared<int>(n);
    auto form = std::make_shared<QWebEngineFrame>();
    auto bestAt = std::make_shared<int>(n);
    auto probe = std::make_shared<QJsonArray>();
    for (int i = 0; i < n; ++i) {
      QWebEngineFrame copy = valid.at(i);
      copy.runJavaScript(
          HeritageIntranetFlow::formHintScript(),
          [this, generation, pending, form, bestAt, copy, invoke, finishEmpty, openSrcs,
           manageFlight, requireForm, then, i, n, jsFrames, srcPage, srcList,
           probe](const QVariant& value) {
            if (generation != m_generation) {
              if (--(*pending) == 0 && manageFlight) m_scriptInFlight = false;
              return;
            }
            const QString hint = value.toString();
            QJsonObject row;
            row.insert(QStringLiteral("url"), copy.url().toString());
            row.insert(QStringLiteral("name"), copy.name());
            row.insert(QStringLiteral("htmlName"), copy.htmlName());
            row.insert(QStringLiteral("hint"), hint);
            row.insert(QStringLiteral("main"), copy.isMainFrame());
            probe->append(row);
            if (hint == QLatin1String("codedeta") && copy.isValid() && i < *bestAt) {
              *form = copy;
              *bestAt = i;
            }
            if (--(*pending) != 0) return;
            QJsonObject wrap;
            wrap.insert(QStringLiteral("cppFrames"), n);
            wrap.insert(QStringLiteral("jsFrames"), jsFrames);
            wrap.insert(QStringLiteral("cpp"), *probe);
            wrap.insert(QStringLiteral("html"), srcList);
            m_lastCppProbe = QString::fromUtf8(QJsonDocument(wrap).toJson(QJsonDocument::Compact));
            if (*bestAt < n) {
              invoke(*form);
              return;
            }
            // children()가 지도 iframe만 주고 다운로드 iframe은 빼도 src 태그는 남는다.
            // n<=1 일 때만 열면 그 경우 시·군에서 영영 not-found 다.
            if (requireForm && openSrcs(srcPage, srcList) > 0) {
              finishEmpty();
              return;
            }
            if (requireForm) {
              finishEmpty();
              return;
            }
            invoke(QWebEngineFrame());
          });
    }
  };

  auto frames = std::make_shared<QVector<QWebEngineFrame>>();
  for (const QWebEngineFrame& frame : allFrames(m_tabs)) appendUnique(frames.get(), frame);

  if (pages.isEmpty()) {
    hintFrames(*frames, 0, nullptr, {});
    return;
  }

  auto pending = std::make_shared<int>(pages.size());
  auto jsMax = std::make_shared<int>(0);
  auto srcPage = std::make_shared<QPointer<QWebEnginePage>>(pages.last());
  auto srcList = std::make_shared<QJsonArray>();
  for (QWebEnginePage* page : pages) {
    QWebEngineFrame main = page->mainFrame();
    if (!main.isValid()) {
      if (--(*pending) == 0) hintFrames(*frames, *jsMax, srcPage->data(), *srcList);
      continue;
    }
    main.runJavaScript(
        HeritageIntranetFlow::frameInventoryScript(),
        [this, generation, pending, frames, jsMax, srcPage, srcList, page, hintFrames,
         manageFlight](const QVariant& value) {
          if (generation != m_generation) {
            if (--(*pending) == 0 && manageFlight) m_scriptInFlight = false;
            return;
          }
          const QJsonObject info = QJsonDocument::fromJson(value.toString().toUtf8()).object();
          *jsMax = qMax(*jsMax, info.value(QStringLiteral("jsFrames")).toInt());
          const QJsonArray list = info.value(QStringLiteral("list")).toArray();
          mergeNamedFrames(page, list, frames.get());
          if (list.size() > srcList->size()) {
            *srcList = list;
            *srcPage = page;
          }
          if (--(*pending) == 0) hintFrames(*frames, *jsMax, srcPage->data(), *srcList);
        });
  }
}

void KaHeritageBrowser::captureOutline(const std::function<void()>& then) {
  runOnPreferredDocument(
      HeritageIntranetFlow::pageOutlineScript(),
      [this, then](const QString& value) {
        m_lastOutline = value;
        QJsonDocument doc = QJsonDocument::fromJson(m_lastOutline.toUtf8());
        if (doc.isObject() && !m_lastCppProbe.isEmpty()) {
          const QJsonDocument probe = QJsonDocument::fromJson(m_lastCppProbe.toUtf8());
          if (probe.isObject()) {
            QJsonObject object = doc.object();
            const QJsonObject extra = probe.object();
            object.insert(QStringLiteral("cppFrames"), extra.value(QStringLiteral("cppFrames")));
            object.insert(QStringLiteral("jsFrames"), extra.value(QStringLiteral("jsFrames")));
            object.insert(QStringLiteral("cpp"), extra.value(QStringLiteral("cpp")));
            object.insert(QStringLiteral("html"), extra.value(QStringLiteral("html")));
            doc.setObject(object);
            m_lastOutline = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
          }
        }
        QString text = doc.isNull() ? m_lastOutline
                                    : QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        // 화면에서 무엇을 찾았는지보다 **무엇을 보냈는지**가 더 확실한 단서다.
        if (m_requestLog) {
          const QStringList sent = m_requestLog->lines();
          if (!sent.isEmpty())
            text += QStringLiteral("\n\n== 사이트가 보낸 요청 (최근 %1개) ==\n%2")
                        .arg(sent.size())
                        .arg(sent.join(QLatin1Char('\n')));
        }
        m_outline->setPlainText(text);
        if (then) then();
      },
      false, false);
}

HeritageStage KaHeritageBrowser::nextStage(HeritageStage stage) const {
  switch (stage) {
    case HeritageStage::Login: return HeritageStage::DismissTutorial;
    case HeritageStage::DismissTutorial: return HeritageStage::OpenDownloadPage;
    case HeritageStage::OpenDownloadPage: return HeritageStage::AgreeTerms;
    case HeritageStage::AgreeTerms: return HeritageStage::SelectDataset;
    case HeritageStage::SelectDataset: return HeritageStage::SelectRegion;
    case HeritageStage::SelectRegion: return HeritageStage::Search;
    case HeritageStage::Search: return HeritageStage::CollectResults;
    case HeritageStage::CollectResults: return HeritageStage::Download;
    default: return HeritageStage::Done;
  }
}

void KaHeritageBrowser::runStage() {
  if (!m_running) return;
  if (++m_waitTicks > kMaxWaitTicks) {
    fail(QStringLiteral("「%1」 단계에서 화면이 예상과 달라 멈췄습니다. 아래 내용을 보고 알려 주세요.")
             .arg(HeritageIntranetFlow::stageName(m_stage)));
    return;
  }
  if (m_scriptInFlight || !m_pageReady) return;
  if (m_settle > 0) {
    --m_settle;
    return;  // 화면이 자리잡기를 기다린다
  }

  // frameset 위에서는 어떤 요소도 못 찾는다. 안쪽 프레임 주소로 직접 들어간다.
  // **페이지가 새로 뜰 때 한 번만** 본다. 매 번 돌리면 단계 스크립트가 영영 돌지 못한다.
  // 프레임 안으로 **이동하지 않는다.** 이 사이트의 루트는 frameset(top 0% + main)이라
  // 안쪽 주소로 옮겨 가면 사이트 자신의 스크립트가 깨지거나 빈 top 프레임이 열린다.
  // 안쪽 문서는 runOnPreferredDocument / cands() 로 이동 없이 읽는다.
  //

  switch (m_stage) {
    case HeritageStage::Login: {
      // 이미 들어가 있으면 다시 로그인하지 않는다(세션이 살아 있을 수 있다).
      runScript(HeritageIntranetFlow::loginProbeScript(), [this](const QString& probe) {
        if (probe == QLatin1String("past-login")) {
          setStage(HeritageStage::DismissTutorial, QStringLiteral("이미 로그인되어 있습니다."));
          return;
        }
        const auto account = HeritageIntranetSettings::credentials();
        // 이 스크립트에는 비밀번호가 들어 있다. 어디에도 보관하거나 찍지 않는다.
        runScript(HeritageIntranetFlow::loginScript(account.username, account.password),
                  [this](const QString& result) {
                    if (result == QLatin1String("no-form")) return;  // 아직 로딩 중일 수 있다
                    if (result == QLatin1String("no-goLogin")) {
                      fail(QStringLiteral("로그인 화면이 바뀌었습니다(goLogin 없음)."));
                      return;
                    }
                    if (result.startsWith(QLatin1String("error:"))) {
                      fail(QStringLiteral("로그인 중 오류가 났습니다: %1").arg(result.mid(6)));
                      return;
                    }
                    if (result == QLatin1String("submitted"))
                      setStage(HeritageStage::DismissTutorial,
                               QStringLiteral("로그인 결과를 기다리는 중입니다."));
                  });
      });
      return;
    }
    case HeritageStage::DismissTutorial: {
      // 로그인이 실제로 됐는지부터 본다. 아직 로그인 칸이 보이면 실패다.
      runScript(HeritageIntranetFlow::loginProbeScript(), [this](const QString& result) {
        if (result == QLatin1String("still-login")) {
          if (m_waitTicks > 8)
            fail(QStringLiteral("로그인이 되지 않았습니다. 아이디·비밀번호를 확인해 주세요."));
          return;
        }
        runScript(HeritageIntranetFlow::dismissTutorialScript(), [this](const QString& closed) {
          // 닫혔는지 확인하기 전에는 다음으로 가지 않는다.
          // 튜토리얼이 떠 있으면 상단 메뉴를 누를 수 없다.
          if (closed == QLatin1String("closed")) {
            m_detailLabel->setText(QStringLiteral("튜토리얼을 닫는 중입니다…"));
            return;  // 다음 차례에 사라졌는지 다시 본다
          }
          if (closed == QLatin1String("none"))
            setStage(HeritageStage::OpenDownloadPage,
                     QStringLiteral("국가유산자료 다운로드 화면을 엽니다."));
        });
      });
      return;
    }
    case HeritageStage::OpenDownloadPage:
      // 눌렀다고 도착한 것으로 치지 않는다. 화면이 실제로 바뀐 뒤에 넘어간다.
      runScript(HeritageIntranetFlow::downloadPageProbeScript(), [this](const QString& probe) {
        if (probe == QLatin1String("ready")) {
          setStage(HeritageStage::AgreeTerms, QStringLiteral("서약서에 동의합니다."));
          return;
        }
        runScript(HeritageIntranetFlow::openDownloadPageScript(), [this](const QString& result) {
          if (result == QLatin1String("not-found"))
            m_detailLabel->setText(QStringLiteral("메뉴에서 「국가유산자료 다운로드」를 찾는 중…"));
          else
            m_detailLabel->setText(QStringLiteral("다운로드 화면을 여는 중…"));
        });
      });
      return;
    case HeritageStage::AgreeTerms:
      runScript(HeritageIntranetFlow::agreeTermsScript(), [this](const QString& result) {
        if (result == QLatin1String("already-open")) {
          // 이미 동의한 세션은 서약서 없이 다운로드 폼이 바로 열린다.
          setStage(HeritageStage::SelectDataset, QStringLiteral("자료 종류를 고릅니다."));
          return;
        }
        if (result == QLatin1String("agreed")) {
          // 동의 시각과 원문을 남긴다. 조용히 지나가지 않는다.
          captureOutline(nullptr);
          emit agreementAccepted(QDateTime::currentDateTime(), m_lastOutline);
          setStage(HeritageStage::SelectDataset, QStringLiteral("자료 종류를 고릅니다."));
          return;
        }
        if (result == QLatin1String("checked-no-confirm"))
          fail(QStringLiteral("서약서에 체크했지만 「확인」 버튼을 찾지 못했습니다."));
      });
      return;
    case HeritageStage::SelectDataset: {
      if (m_datasetIndex >= m_datasets.size()) {
        setStage(HeritageStage::Done, QStringLiteral("모두 마쳤습니다."));
        m_running = false;
        m_poll->stop();
        emit allFinished();
        return;
      }
      runScript(HeritageIntranetFlow::selectDatasetScript(m_datasets.at(m_datasetIndex)),
                [this](const QString& result) {
                  if (result != QLatin1String("selected")) return;
                  runScript(HeritageIntranetFlow::resultPageInfoScript(),
                            [this](const QString& info) {
                              const int total =
                                  QJsonDocument::fromJson(info.toUtf8()).object()
                                      .value(QStringLiteral("total")).toInt(-1);
                              if (total <= 0) {
                                m_detailLabel->setText(QStringLiteral("검색 전 건수를 읽는 중…"));
                                if (m_waitTicks > 15)
                                  fail(QStringLiteral(
                                      "검색 전 건수를 읽지 못해 시·군 조건을 확인할 수 없습니다."));
                                return;
                              }
                              m_totalBeforeSearch = total;
                              setStage(HeritageStage::SelectRegion,
                                       QStringLiteral("%1 %2 을(를) 고릅니다.").arg(m_sido, m_city));
                            },
                            true);
                },
                true);
      return;
    }
    case HeritageStage::SelectRegion:
      runScript(HeritageIntranetFlow::selectRegionScript(m_sido, m_city),
                [this](const QString& result) {
                  if (result == QLatin1String("no-sido")) {
                    fail(QStringLiteral("시·도 목록에서 「%1」을 찾지 못했습니다. 표기가 다릅니다.")
                             .arg(m_sido));
                    return;
                  }
                  if (result == QLatin1String("not-found")) {
                    m_detailLabel->setText(QStringLiteral("다운로드 폼에서 시·도 칸을 찾는 중…"));
                    if (m_waitTicks > 20)
                      fail(QStringLiteral("다운로드 폼에서 시·도 칸을 찾지 못했습니다."));
                    return;
                  }
                  if (result == QLatin1String("selected")) {
                    // 「골랐다」를 한 번 보고 믿지 않는다. 탭이 바뀌면 폼이 다시 그려지고
                    // 시/군 목록은 ajax 로 나중에 채워진다(2026-09-12: 1초 만에 selected 를
                    // 받았지만 실제로는 비어 전국을 요청했다). 값이 남아 있는지 다시 본다.
                    runScript(HeritageIntranetFlow::verifyRegionScript(),
                              [this](const QString& check) {
                                if (!check.startsWith(QLatin1String("ok:"))) {
                                  m_detailLabel->setText(
                                      QStringLiteral("시·군이 아직 폼에 남지 않았습니다. 다시 고릅니다…"));
                                  return;  // 다음 차례에 다시 고른다
                                }
                                setStage(HeritageStage::Search,
                                         QStringLiteral("검색합니다. (%1)").arg(check.mid(3)));
                              },
                              true);
                    return;
                  }
                  if (result == QLatin1String("no-sigungu-value")) {
                    fail(QStringLiteral("시·군 값이 폼에 들어가지 않았습니다. "
                                        "전국 자료를 받지 않기 위해 멈춥니다."));
                    return;
                  }
                  // 시/군/구는 시/도를 고른 뒤 Ajax 로 채워진다. 몇 개나 찼는지 보여 준다.
                  if (result == QLatin1String("sigungu-pending")) {
                    m_detailLabel->setText(QStringLiteral("시·군·구 칸이 아직 없습니다…"));
                    if (m_waitTicks > 20)
                      fail(QStringLiteral("다운로드 폼에서 시·군·구 칸을 찾지 못했습니다."));
                    return;
                  }
                  if (result.startsWith(QLatin1String("sigungu:"))) {
                    const int count = result.mid(8).toInt();
                    m_detailLabel->setText(
                        QStringLiteral("시·군·구 목록 %1개에서 「%2」을(를) 찾는 중…")
                            .arg(count)
                            .arg(m_city));
                    if (count > 0 && m_waitTicks > 12)
                      fail(QStringLiteral("시·군·구 목록 %1개에 「%2」이(가) 없습니다. 표기를 확인해야 합니다.")
                               .arg(count)
                               .arg(m_city));
                  }
                },
                true);
      return;
    case HeritageStage::Search:
      runScript(HeritageIntranetFlow::searchScript(),
                [this](const QString& result) {
                  if (result == QLatin1String("not-found")) {
                    if (m_waitTicks > 15)
                      fail(QStringLiteral("다운로드 폼에서 「검색」을 찾지 못했습니다."));
                    return;
                  }
                  if (result != QLatin1String("searching")) return;
                  // 「10개씩 보기」를 건드리지 않는다. 목록이 다시 그려지면서
                  // 전체다운로드 함수가 읽는 요소가 사라져 null.value 로 터진다(2026-09-12).
                  // 전체다운로드는 쪽 크기와 무관하게 검색결과 전부를 받는다.
                  setStage(HeritageStage::CollectResults, QStringLiteral("결과를 모읍니다."));
                },
                true);
      return;
    case HeritageStage::CollectResults:
      runScript(HeritageIntranetFlow::resultPageInfoScript(),
                [this](const QString& result) {
        const QJsonObject info = QJsonDocument::fromJson(result.toUtf8()).object();
        const int rows = info.value(QStringLiteral("rows")).toInt();
        if (rows <= 0) return;  // 아직 그리는 중일 수 있다
        const int total = info.value(QStringLiteral("total")).toInt(-1);
        if (total <= 0 || m_totalBeforeSearch <= 0) {
          // 아직 갱신 중일 수 있다. 건수를 못 읽었다고 바로 포기하지 않는다.
          if (m_waitTicks > 20)
            fail(QStringLiteral("검색 건수를 읽지 못해 전국 받기를 하지 않습니다"
                                "(검색 전 %1건 · 검색 후 %2건).")
                     .arg(m_totalBeforeSearch)
                     .arg(total));
          return;
        }
        if (total == m_totalBeforeSearch) {
          // 검색 직후에는 화면이 아직 옛 건수를 보이고 있다(2026-09-12: 8302 를 읽고 막았는데
          // 실제로는 곧 90 으로 바뀌었다). 몇 번 더 보고 나서 판단한다.
          m_detailLabel->setText(
              QStringLiteral("검색 결과를 기다립니다… (아직 %1건)").arg(total));
          if (m_waitTicks > 20)
            fail(QStringLiteral("시·군 조건이 걸리지 않았습니다(검색결과가 %1건 그대로입니다). "
                                "전국 자료를 받지 않기 위해 멈춥니다.")
                     .arg(total));
          return;
        }
        m_lastPage = qMax(1, info.value(QStringLiteral("lastPage")).toInt(1));
        m_currentPage = 1;
        m_currentFiles.clear();
        m_pendingDownloads = 0;
        m_idleTicks = 0;
        m_downloadMode.clear();
        setStage(HeritageStage::Download,
                 QStringLiteral("%1 — 결과 %2줄 · 전체 %3쪽, 내려받습니다.")
                     .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)))
                     .arg(rows)
                     .arg(m_lastPage));
                },
                true);
      return;

    case HeritageStage::Download: {
      if (m_pendingDownloads > 0) {
        m_idleTicks = 0;
        m_waitTicks = 0;  // 큰 파일은 오래 걸린다. 받는 중이면 기다린다
        return;
      }
      if (!m_downloadMode.isEmpty()) {
        // 누르고 난 뒤 잠깐 조용한지 본다. 바로 끝났다고 하지 않는다.
        if (++m_idleTicks < 6) return;
        if (m_currentFiles.isEmpty()) {
          fail(QStringLiteral("「%1」에서 내려받기를 눌렀지만 파일이 오지 않았습니다.")
                   .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
          return;
        }
        // 쪽마다 받는 방식이면 다음 쪽으로 넘긴다. 한 쪽만 받고 끝내지 않는다.
        if (m_downloadMode == QLatin1String("selected") && m_currentPage < m_lastPage) {
          const int next = m_currentPage + 1;
          runScript(HeritageIntranetFlow::goToPageScript(next),
                    [this, next](const QString& moved) {
                      if (moved == QLatin1String("moved")) {
                        m_currentPage = next;
                        m_downloadMode.clear();
                        m_idleTicks = 0;
                        m_waitTicks = 0;
                        m_detailLabel->setText(
                            QStringLiteral("%1쪽 / %2쪽").arg(next).arg(m_lastPage));
                      } else {
                        m_lastPage = m_currentPage;
                      }
                    },
                    true);
          return;
        }
        // 받은 것이 진짜인지 확인하고 넘어간다. 0바이트 빈 파일을 받은 적이 있다(2026-09-12).
        QStringList good;
        qint64 totalBytes = 0;
        for (const QString& path : std::as_const(m_currentFiles)) {
          const QFileInfo info(path);
          if (!info.exists() || info.size() <= 0) {
            logLine(QStringLiteral("빈 파일이라 버립니다: %1").arg(path));
            QFile::remove(path);
            continue;
          }
          good << path;
          totalBytes += info.size();
        }
        if (good.isEmpty()) {
          fail(QStringLiteral("「%1」에서 받은 파일이 모두 비어 있습니다.")
                   .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
          return;
        }
        m_currentFiles = good;
        logLine(QStringLiteral("확인: %1 파일 %2개 · %3KB")
                    .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)))
                    .arg(good.size())
                    .arg(totalBytes / 1024));

        // 이 종류는 끝났다. 지도에 올리게 넘기고 다음 종류로 간다.
        emit datasetReady(m_datasets.at(m_datasetIndex), m_currentFiles);
        ++m_datasetIndex;
        if (m_datasetIndex >= m_datasets.size()) {
          setStage(HeritageStage::Done, QStringLiteral("여섯 종을 모두 받았습니다."));
          m_running = false;
          m_poll->stop();
          emit allFinished();
          return;
        }
        // 다음 종류로 갈 때 기준 건수를 지운다. 이전 종류의 값이 남으면
        // 전국 가드가 「달라졌으니 조건이 걸렸다」고 잘못 판단한다(2026-09-12).
        m_totalBeforeSearch = -1;
        setStage(HeritageStage::SelectDataset,
                 QStringLiteral("다음 자료: %1")
                     .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
        return;
      }
      // 주소를 우리가 만들면 인코딩이 어긋난다(searchParams 가 %25EC… 로 이중 인코딩되어
      // 서버가 0바이트를 준다. 2026-09-12 두 번 당했다).
      // **사이트가 제 방식대로 만들게 둔다.** 함수는 #searchForm 이 있는 창에서 부른다.
      // 사이트가 제 팝업을 열면 그 창은 내려받기용이라 곧 닫는다 — 창은 하나로 유지한다.
      runScript(HeritageIntranetFlow::downloadAllScript(),
                [this](const QString& result) {
                  if (result == QLatin1String("not-found")) return;
                  if (result == QLatin1String("no-region")) {
                    fail(QStringLiteral("「%1」에서 시·군이 폼에 들어가지 않았습니다. "
                                        "전국 자료를 받지 않기 위해 멈춥니다.")
                             .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
                    return;
                  }
                  if (result.startsWith(QLatin1String("error:"))) {
                    fail(QStringLiteral("내려받기 중 오류가 났습니다: %1").arg(result.mid(6)));
                    return;
                  }
                  m_downloadMode = result;
                  m_idleTicks = 0;
                  m_waitTicks = 0;
                  m_detailLabel->setText(QStringLiteral("내려받는 중입니다…"));
                },
                true);
      return;
    }
    default:
      return;
  }
}

void KaHeritageBrowser::handleDownload(QWebEngineDownloadRequest* request) {
  if (!request || request->state() != QWebEngineDownloadRequest::DownloadRequested) return;
  if (m_downloadRoot.isEmpty()) {
    request->cancel();
    fail(QStringLiteral("저장할 폴더가 없어 내려받기를 취소했습니다."));
    return;
  }
  const QString directory =
      QDir(m_downloadRoot).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
  if (!QDir().mkpath(directory)) {
    request->cancel();
    fail(QStringLiteral("저장 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요."));
    return;
  }
  const QString name = safeName(request->suggestedFileName());
  request->setDownloadDirectory(directory);
  request->setDownloadFileName(name);
  ++m_pendingDownloads;
  m_idleTicks = 0;
  connect(request, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, request]() {
    if (!request->isFinished()) return;
    if (m_pendingDownloads > 0) --m_pendingDownloads;
    if (request->state() != QWebEngineDownloadRequest::DownloadCompleted) {
      // 사이트는 다운로드를 팝업 창(openWindow)으로 연다. 그 창의 빈 요청 하나가
      // 취소로 끝나는 일이 있는데, **이미 받은 파일이 있으면 실패가 아니다.**
      if (!m_currentFiles.isEmpty()) {
        m_detailLabel->setText(QStringLiteral("받은 파일 %1개 (빈 요청 하나는 건너뜁니다)")
                                   .arg(m_currentFiles.size()));
        return;
      }
      fail(QStringLiteral("파일을 끝까지 받지 못했습니다."));
      return;
    }
    const QString path =
        QDir(request->downloadDirectory()).filePath(request->downloadFileName());
    m_currentFiles << path;
    logLine(QStringLiteral("받음: %1").arg(path));
    // 사이트가 내려받기용으로 연 팝업 탭은 곧 닫는다. 창을 하나로 유지한다.
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
      auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
      if (!view || view == m_mainView) continue;
      m_tabs->removeTab(i);
      view->deleteLater();
    }
    m_idleTicks = 0;
    m_detailLabel->setText(QStringLiteral("받은 파일 %1개").arg(m_currentFiles.size()));
    emit fileDownloaded(path);
  });
  request->accept();
}

QWebEngineView* KaHeritageBrowser::addPage(QWebEnginePage* opener) {
  Q_UNUSED(opener);
  auto* view = new QWebEngineView(m_tabs);
  auto* page = new HeritagePage(
      m_profile, view, [this](QWebEnginePage*) { return addPage(); },
      [this](const QString& message) {
        // 사이트가 띄운 알림. 단계 표시에 남겨 무엇 때문에 멈췄는지 보이게 한다.
        const QString one = QString(message).replace(QLatin1Char('\n'), QLatin1Char(' ')).trimmed();
        m_lastAlert = one;
        m_detailLabel->setText(QStringLiteral("사이트 알림: %1").arg(one.left(120)));
      });
  view->setPage(page);
  const int index = m_tabs->addTab(view, QStringLiteral("국가유산 인트라넷"));
  m_tabs->setCurrentIndex(index);
  // 내려받기용으로 여는 탭은 화면이 뜨는 것이 아니라 파일이 떨어지는 것이라
  // loadFinished 가 오지 않는다. 그 탭 때문에 준비 상태를 내리면 흐름이 영영 멈춘다
  // (2026-09-12: 「받은 파일 1개」에서 다음 종류로 넘어가지 못했다).
  // 그래서 **작업 화면(첫 탭)만** 준비 상태를 관리한다.
  const bool isMain = (m_mainView == nullptr);
  if (isMain) m_mainView = view;
  connect(view, &QWebEngineView::loadStarted, this, [this, view]() {
    if (view != m_mainView) return;
    m_pageReady = false;
    m_frameChecked = false;  // 새 화면이면 프레임을 다시 본다
  });
  connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
    if (!ok || view != m_mainView) return;
    m_pageReady = true;
    m_frameChecked = false;
    m_waitTicks = 0;
    m_tabs->setCurrentWidget(view);
  });
  connect(page, &QWebEnginePage::windowCloseRequested, this, [this, view]() {
    const int at = m_tabs->indexOf(view);
    if (at >= 0) {
      m_tabs->removeTab(at);
      view->deleteLater();
    }
  });
  connect(view, &QWebEngineView::renderProcessTerminated, this,
          [this](QWebEnginePage::RenderProcessTerminationStatus, int) {
            fail(QStringLiteral("인트라넷 연결이 끊겼습니다. 다시 눌러 주세요."));
          });
  return view;
}

QWebEnginePage* KaHeritageBrowser::activePage() const {
  if (!m_tabs) return nullptr;
  // 가장 최근에 뜬 인트라넷 페이지. 팝업이 떠 있으면 그쪽이 먼저다.
  for (int i = m_tabs->count() - 1; i >= 0; --i) {
    auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
    if (!view) continue;
    if (view->url().host().endsWith(QStringLiteral("gis-heritage.go.kr"))) return view->page();
  }
  auto* current = qobject_cast<QWebEngineView*>(m_tabs->currentWidget());
  return current ? current->page() : nullptr;
}
