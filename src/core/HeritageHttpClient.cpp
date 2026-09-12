#include "HeritageHttpClient.h"

#include "HeritageIntranetFlow.h"

#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QUrlQuery>

namespace {
constexpr int kTimeoutMs = 30000;

QString userAgent() {
  return QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3");
}
}  // namespace

HeritageHttpClient::HeritageHttpClient(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {
  m_nam->setCookieJar(new QNetworkCookieJar(m_nam));
}

HeritageHttpClient::~HeritageHttpClient() = default;

void HeritageHttpClient::setCookies(const QList<QNetworkCookie>& cookies) {
  if (cookies.isEmpty()) return;
  m_nam->cookieJar()->setCookiesFromUrl(cookies, HeritageIntranetFlow::portalUrl());
  m_hasSession = true;
}

bool HeritageHttpClient::hasSession() const { return m_hasSession; }

void HeritageHttpClient::setDumpDir(const QString& dir) { m_dumpDir = dir; }

QString HeritageHttpClient::dump(const QString& tag, const QByteArray& html) const {
  if (m_dumpDir.isEmpty() || html.isEmpty()) return {};
  if (!QDir().mkpath(m_dumpDir)) return {};
  const QString path =
      QDir(m_dumpDir)
          .filePath(QStringLiteral("%1-%2.html")
                        .arg(tag,
                             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return {};
  file.write(html);
  return path;
}

QByteArray HeritageHttpClient::buildBody(const HeritageForm& form, const QString& sidoValue,
                                         const QString& sigunguValue,
                                         const QMap<QString, QString>& extra) {
  QUrlQuery q;
  // 폼에 있던 칸을 그대로 되돌려 보낸다. 서버가 기대하는 hidden 값이 있다.
  for (auto it = form.fields.constBegin(); it != form.fields.constEnd(); ++it) {
    QString value = it.value();
    if (!form.sidoField.isEmpty() && it.key() == form.sidoField) value = sidoValue;
    if (!form.sigunguField.isEmpty() && it.key() == form.sigunguField) value = sigunguValue;
    q.addQueryItem(QUrl::toPercentEncoding(it.key()), QUrl::toPercentEncoding(value));
  }
  // 폼에 없던 칸이면 새로 넣는다(select 가 form 밖에 있는 화면 대비).
  auto ensure = [&](const QString& name, const QString& value) {
    if (name.isEmpty() || form.fields.contains(name)) return;
    q.addQueryItem(QUrl::toPercentEncoding(name), QUrl::toPercentEncoding(value));
  };
  ensure(form.sidoField, sidoValue);
  ensure(form.sigunguField, sigunguValue);
  for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
    q.addQueryItem(QUrl::toPercentEncoding(it.key()), QUrl::toPercentEncoding(it.value()));
  return q.toString(QUrl::FullyEncoded).toUtf8();
}

QUrl HeritageHttpClient::resolveAction(const QUrl& pageUrl, const HeritageForm& form) {
  if (form.action.trimmed().isEmpty()) return pageUrl;
  return pageUrl.resolved(QUrl(form.action.trimmed()));
}

QNetworkReply* HeritageHttpClient::post(const QUrl& url, const QByteArray& body) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded; charset=UTF-8"));
  req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
  req.setRawHeader("Accept", "text/html,application/xhtml+xml,*/*");
  // 사이트는 jQuery 의 $.ajax 로 이 주소들을 부른다(tabContentAjax).
  // 그 요청에는 이 헤더가 붙는다. 없으면 서버가 404 로 돌려보낸다.
  req.setRawHeader("X-Requested-With", "XMLHttpRequest");
  // Referer 는 실제로 그 화면에서 부른 것처럼 로그인 후 화면 주소로 둔다.
  req.setRawHeader("Referer",
                   HeritageIntranetFlow::portalUrl()
                       .resolved(QUrl(QStringLiteral("/ngis/common/main.do?httpYn=Y")))
                       .toString()
                       .toUtf8());
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::SameOriginRedirectPolicy);
  req.setTransferTimeout(kTimeoutMs);
  return m_nam->post(req, body);
}

void HeritageHttpClient::fetchDownloadPage() {
  if (!m_hasSession) {
    emit failed(QStringLiteral("로그인 세션이 없습니다. 먼저 로그인해야 합니다."));
    return;
  }
  // ① 껍데기. 여기에는 폼이 없다(2026-09-12 실제 응답으로 확인).
  //    사이트 JS 가 이어서 tabContentAjax("/user/data/heritageDownloadList.do", …) 를 부른다.
  const QUrl shell = HeritageIntranetFlow::portalUrl().resolved(
      QUrl(HeritageIntranetFlow::downloadPagePath()));
  QNetworkReply* first = post(shell, QByteArray());
  connect(first, &QNetworkReply::finished, this, [this, first]() {
    first->deleteLater();
    if (first->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("다운로드 화면을 받지 못했습니다: %1").arg(first->errorString()));
      return;
    }
    // ② 목록 조각. 검색 폼(#searchForm)과 시/도·시/군/구 칸이 여기 있다.
    m_pageUrl = HeritageIntranetFlow::portalUrl().resolved(
        QUrl(HeritageIntranetFlow::downloadListPath()));
    // 사이트 JS 는 늘 params 를 함께 보낸다. 빈 몸통으로는 화면이 안 나올 수 있다.
    QUrlQuery seed;
    seed.addQueryItem(QStringLiteral("pageMenuId"), QStringLiteral("DAT010010"));
    seed.addQueryItem(QStringLiteral("pageIndex"), QStringLiteral("1"));
    QNetworkReply* list = post(m_pageUrl, seed.toString(QUrl::FullyEncoded).toUtf8());
    connect(list, &QNetworkReply::finished, this, [this, list]() {
      list->deleteLater();
      const int code =
          list->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (list->error() != QNetworkReply::NoError) {
        const QString saved = dump(QStringLiteral("download-list-error"), list->readAll());
        QString why = QStringLiteral("자료 목록을 받지 못했습니다(HTTP %1): %2")
                          .arg(code)
                          .arg(list->errorString());
        if (!saved.isEmpty()) why += QStringLiteral("\n받은 화면: %1").arg(saved);
        emit failed(why);
        return;
      }
      const QByteArray html = list->readAll();
      m_form = HeritageFormParser::parse(html);
      if (!m_form.ok()) {
        // 무엇이 왔는지 추측하지 않는다 — 받은 HTML 을 그대로 남긴다.
        const QString saved = dump(QStringLiteral("download-list"), html);
        QString why = QStringLiteral("자료 목록에서 검색 폼을 읽지 못했습니다. %1 (받은 크기 %2바이트)")
                          .arg(m_form.warnings.join(QLatin1Char(' ')))
                          .arg(html.size());
        if (!saved.isEmpty()) why += QStringLiteral("\n받은 화면: %1").arg(saved);
        emit failed(why);
        return;
      }
      m_totalBeforeSearch = HeritageFormParser::resultCount(html);
      emit pageReady(m_form, m_totalBeforeSearch);
    });
  });
}

void HeritageHttpClient::search(const QString& sido, const QString& city) {
  if (!m_form.ok()) {
    emit failed(QStringLiteral("검색 폼을 아직 읽지 못했습니다."));
    return;
  }
  const QString sidoValue = HeritageFormParser::matchOption(m_form.sidoOptions, sido);
  if (sidoValue.isEmpty()) {
    emit failed(QStringLiteral("시·도 목록에서 「%1」을 찾지 못했습니다.").arg(sido));
    return;
  }
  // 시/군/구 목록은 시/도를 고른 뒤 채워지는 화면이 있다. 없으면 시/도만으로 한 번 조회한다.
  const QString cityValue = HeritageFormParser::matchOption(m_form.sigunguOptions, city);

  // 사이트 JS 와 같은 규칙: codedetaCd 값이 "A,B" 꼴이면 뒤쪽을 codeCd 로 함께 보낸다.
  QMap<QString, QString> extra;
  const QStringList parts = sidoValue.split(QLatin1Char(','));
  extra.insert(QStringLiteral("codeCd"), parts.size() > 1 ? parts.at(1) : QString());

  const QUrl url = resolveAction(m_pageUrl, m_form);
  QNetworkReply* reply = post(url, buildBody(m_form, sidoValue, cityValue, extra));
  connect(reply, &QNetworkReply::finished, this, [this, reply, city, cityValue]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("검색 요청이 실패했습니다: %1").arg(reply->errorString()));
      return;
    }
    const QByteArray html = reply->readAll();
    const int count = HeritageFormParser::resultCount(html);

    // 시/군 조건이 걸리지 않았는데 받으면 전국 자료를 내려받는다. 그건 하지 않는다.
    if (count > 0 && m_totalBeforeSearch > 0 && count == m_totalBeforeSearch) {
      emit failed(QStringLiteral("시·군 조건이 걸리지 않았습니다(검색결과가 %1건 그대로입니다). "
                                 "전국 자료를 받지 않기 위해 멈춥니다.")
                      .arg(count));
      return;
    }
    if (cityValue.isEmpty()) {
      // 시/군을 못 골랐으면 시/도 전체다. 조용히 넘어가지 않는다.
      emit failed(QStringLiteral("시·군·구 목록에서 「%1」을 찾지 못했습니다. "
                                 "시·도 전체를 받지 않기 위해 멈춥니다.")
                      .arg(city));
      return;
    }
    emit searchReady(count, html);
  });
}
