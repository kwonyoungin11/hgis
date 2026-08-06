#include "LocationSearch.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

static QString secretsPath() {
  const QStringList cands = {
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/secrets.ini")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/secrets.ini")),
    QDir::current().filePath(QStringLiteral("config/secrets.ini")),
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("secrets.ini")),
  };
  for (const QString& p : cands) {
    if (QFile::exists(p)) return p;
  }
  return cands.first();
}

static QString readKeyFromSecretsFile() {
  const QString path = secretsPath();
  if (!QFile::exists(path)) return {};
  QSettings ini(path, QSettings::IniFormat);
  QString k = ini.value(QStringLiteral("vworld/apiKey")).toString().trimmed();
  if (k.isEmpty()) k = ini.value(QStringLiteral("apiKey")).toString().trimmed();
  return k;
}

LocationSearch::LocationSearch(QObject* parent) : QObject(parent) {}

QString LocationSearch::vworldApiKey() {
  const QByteArray env = qgetenv("VWORLD_API_KEY");
  if (!env.isEmpty()) return QString::fromUtf8(env);
  const QString fromFile = readKeyFromSecretsFile();
  if (!fromFile.isEmpty()) return fromFile;
  return QSettings().value(QStringLiteral("vworld/apiKey")).toString().trimmed();
}

void LocationSearch::setVworldApiKey(const QString& key) {
  const QString k = key.trimmed();
  QSettings().setValue(QStringLiteral("vworld/apiKey"), k);
  const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/secrets.ini"));
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSettings ini(path, QSettings::IniFormat);
  ini.setValue(QStringLiteral("vworld/apiKey"), k);
  ini.sync();
}

void LocationSearch::search(const QString& query) {
  const QString q = query.trimmed();
  if (q.isEmpty()) {
    emit failed(QStringLiteral("검색어를 입력하세요"));
    return;
  }
  if (m_pending) {
    emit failed(QStringLiteral("이전 검색이 진행 중입니다"));
    return;
  }
  m_pending = true;
  if (!vworldApiKey().isEmpty())
    searchVworld(q);
  else
    searchNominatim(q);
}

void LocationSearch::searchNominatim(const QString& query) {
  QUrl url(QStringLiteral("https://nominatim.openstreetmap.org/search"));
  QUrlQuery uq;
  uq.addQueryItem(QStringLiteral("q"), query);
  uq.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));
  uq.addQueryItem(QStringLiteral("limit"), QStringLiteral("12"));
  uq.addQueryItem(QStringLiteral("countrycodes"), QStringLiteral("kr"));
  uq.addQueryItem(QStringLiteral("accept-language"), QStringLiteral("ko"));
  url.setQuery(uq);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("ka-hgis/0.3 (Korean archaeology HGIS; contact: local)"));
  req.setRawHeader("Accept", "application/json");

  QNetworkReply* reply = m_nam.get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    m_pending = false;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("검색 네트워크 오류: %1").arg(reply->errorString()));
      return;
    }
    handleNominatim(reply->readAll());
  });
}

void LocationSearch::handleNominatim(const QByteArray& body) {
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isArray()) {
    emit failed(QStringLiteral("검색 응답 형식 오류"));
    return;
  }
  QVector<LocationHit> hits;
  for (const QJsonValue& v : doc.array()) {
    const QJsonObject o = v.toObject();
    LocationHit h;
    h.title = o.value(QStringLiteral("display_name")).toString();
    h.detail = o.value(QStringLiteral("type")).toString() + QStringLiteral(" / ")
               + o.value(QStringLiteral("class")).toString();
    h.lat = o.value(QStringLiteral("lat")).toString().toDouble();
    h.lon = o.value(QStringLiteral("lon")).toString().toDouble();
    const QJsonArray bb = o.value(QStringLiteral("boundingbox")).toArray();
    if (bb.size() == 4) {
      h.south = bb.at(0).toString().toDouble();
      h.north = bb.at(1).toString().toDouble();
      h.west = bb.at(2).toString().toDouble();
      h.east = bb.at(3).toString().toDouble();
      h.hasBbox = true;
    }
    if (!h.title.isEmpty()) hits.push_back(h);
  }
  if (hits.isEmpty())
    emit failed(QStringLiteral("검색 결과 없음 (주소·지번·상호를 다시 입력)"));
  else
    emit finished(hits);
}

void LocationSearch::searchVworld(const QString& query) {
  QUrl url(QStringLiteral("https://api.vworld.kr/req/search"));
  QUrlQuery uq;
  uq.addQueryItem(QStringLiteral("service"), QStringLiteral("search"));
  uq.addQueryItem(QStringLiteral("request"), QStringLiteral("search"));
  uq.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0"));
  uq.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:4326"));
  uq.addQueryItem(QStringLiteral("size"), QStringLiteral("12"));
  uq.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
  uq.addQueryItem(QStringLiteral("query"), query);
  uq.addQueryItem(QStringLiteral("type"), QStringLiteral("place"));
  uq.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("errorformat"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("key"), vworldApiKey());
  url.setQuery(uq);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ka-hgis/0.3"));
  QNetworkReply* reply = m_nam.get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      m_pending = false;
      searchNominatim(query);
      return;
    }
    const QByteArray body = reply->readAll();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QString status = root.value(QStringLiteral("response")).toObject()
                               .value(QStringLiteral("status")).toString();
    if (status != QLatin1String("OK")) {
      m_pending = false;
      searchNominatim(query);
      return;
    }
    m_pending = false;
    handleVworld(body);
  });
}

void LocationSearch::handleVworld(const QByteArray& body) {
  const QJsonObject resp = QJsonDocument::fromJson(body).object().value(QStringLiteral("response")).toObject();
  const QJsonArray items = resp.value(QStringLiteral("result")).toObject()
                               .value(QStringLiteral("items")).toArray();
  QVector<LocationHit> hits;
  for (const QJsonValue& v : items) {
    const QJsonObject o = v.toObject();
    LocationHit h;
    h.title = o.value(QStringLiteral("title")).toString();
    if (h.title.isEmpty()) h.title = o.value(QStringLiteral("address")).toObject()
                                         .value(QStringLiteral("road")).toString();
    const QJsonObject addr = o.value(QStringLiteral("address")).toObject();
    h.detail = addr.value(QStringLiteral("parcel")).toString();
    if (h.detail.isEmpty()) h.detail = addr.value(QStringLiteral("road")).toString();
    const QJsonObject pt = o.value(QStringLiteral("point")).toObject();
    h.lon = pt.value(QStringLiteral("x")).toString().toDouble();
    h.lat = pt.value(QStringLiteral("y")).toString().toDouble();
    if (h.lon == 0 && h.lat == 0) {
      h.lon = pt.value(QStringLiteral("x")).toDouble();
      h.lat = pt.value(QStringLiteral("y")).toDouble();
    }
    if (!h.title.isEmpty() && h.lon != 0) hits.push_back(h);
  }
  if (hits.isEmpty())
    emit failed(QStringLiteral("VWorld 결과 없음"));
  else
    emit finished(hits);
}
