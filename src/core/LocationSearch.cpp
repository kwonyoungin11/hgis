#include "LocationSearch.h"
#include "VworldSettings.h"
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
#include <cmath>

static bool readCoordinate(const QJsonValue& value, double& coordinate) {
  bool ok = value.isDouble();
  coordinate = value.isString() ? value.toString().toDouble(&ok) : value.toDouble();
  return ok && std::isfinite(coordinate);
}

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

LocationSearch::LocationSearch(QObject* parent)
    : LocationSearch(std::make_unique<QNetworkAccessManager>(), 20000, parent) {}

LocationSearch::LocationSearch(std::unique_ptr<QNetworkAccessManager> network, int timeoutMs,
                               QObject* parent)
    : QObject(parent), m_nam(network ? std::move(network)
                                   : std::make_unique<QNetworkAccessManager>()),
      m_timeoutMs(timeoutMs > 0 ? timeoutMs : 20000) {
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, [this]() {
    cancel();
    emit failed(QStringLiteral("위치 검색 서버의 응답 시간이 초과되었습니다. 인터넷 연결을 확인한 뒤 다시 검색하세요."));
  });
}

LocationSearch::~LocationSearch() { cancel(); }

void LocationSearch::cancel() {
  m_deadline.stop();
  m_pending = false;
  if (m_reply) {
    QNetworkReply* reply = m_reply.data();
    m_reply.clear();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }
}

void LocationSearch::completeRequest() {
  m_deadline.stop();
  m_reply.clear();
  m_pending = false;
}

QString LocationSearch::vworldApiKey() {
  const QByteArray env = qgetenv("VWORLD_API_KEY");
  if (!env.isEmpty()) return QString::fromUtf8(env).trimmed();
  return VworldSettings::loadApiKey();
}

void LocationSearch::setVworldApiKey(const QString& key) {
  VworldSettings::saveApiKey(key);
}

void LocationSearch::search(const QString& query) {
  const QString q = query.trimmed();
  if (q.isEmpty()) {
    emit failed(QStringLiteral("검색어를 입력하세요"));
    return;
  }
  if (m_pending) {
    emit failed(QStringLiteral("이전 위치를 검색하고 있습니다. 검색이 끝난 뒤 다시 검색하세요."));
    return;
  }
  m_pending = true;
  // One deadline covers both providers; a slow fallback cannot extend it indefinitely.
  m_deadline.start(m_timeoutMs);
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
  req.setTransferTimeout(m_timeoutMs);

  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    completeRequest();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("위치 검색 서버에 연결하지 못했습니다. 인터넷 연결을 확인하고 잠시 후 다시 검색하세요."));
      return;
    }
    handleNominatim(reply->readAll());
  });
}

void LocationSearch::handleNominatim(const QByteArray& body) {
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isArray()) {
    emit failed(QStringLiteral("위치 검색 서버가 올바른 결과를 보내지 않았습니다. 잠시 후 다시 검색하세요."));
    return;
  }
  QVector<LocationHit> hits;
  for (const QJsonValue& v : doc.array()) {
    const QJsonObject o = v.toObject();
    LocationHit h;
    h.title = o.value(QStringLiteral("display_name")).toString();
    h.detail = o.value(QStringLiteral("type")).toString() + QStringLiteral(" / ")
               + o.value(QStringLiteral("class")).toString();
    if (!readCoordinate(o.value(QStringLiteral("lat")), h.lat)
        || !readCoordinate(o.value(QStringLiteral("lon")), h.lon)
        || h.lat < -90 || h.lat > 90 || h.lon < -180 || h.lon > 180)
      continue;
    const QJsonArray bb = o.value(QStringLiteral("boundingbox")).toArray();
    if (bb.size() == 4) {
      h.hasBbox = readCoordinate(bb.at(0), h.south)
                  && readCoordinate(bb.at(1), h.north)
                  && readCoordinate(bb.at(2), h.west)
                  && readCoordinate(bb.at(3), h.east)
                  && h.south >= -90 && h.north <= 90 && h.south < h.north
                  && h.west >= -180 && h.east <= 180 && h.west < h.east;
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
  req.setTransferTimeout(m_timeoutMs);
  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      searchNominatim(query);
      return;
    }
    const QByteArray body = reply->readAll();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QString status = root.value(QStringLiteral("response")).toObject()
                               .value(QStringLiteral("status")).toString();
    if (status != QLatin1String("OK")) {
      searchNominatim(query);
      return;
    }
    completeRequest();
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
    if (!readCoordinate(pt.value(QStringLiteral("x")), h.lon)
        || !readCoordinate(pt.value(QStringLiteral("y")), h.lat)
        || h.lat < -90 || h.lat > 90 || h.lon < -180 || h.lon > 180)
      continue;
    if (!h.title.isEmpty()) hits.push_back(h);
  }
  if (hits.isEmpty())
    emit failed(QStringLiteral("검색한 위치를 찾지 못했습니다. 주소·지번·장소 이름을 확인한 뒤 다시 검색하세요."));
  else
    emit finished(hits);
}
