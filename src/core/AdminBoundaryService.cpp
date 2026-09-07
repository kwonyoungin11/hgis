#include "AdminBoundaryService.h"
#include "VworldSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

AdminBoundaryService::AdminBoundaryService(QObject* parent) : QObject(parent) {}

QString AdminBoundaryService::attrFilter(const QString& sido, const QString& city,
                                         const QString& dong) {
  QStringList parts;
  if (!sido.trimmed().isEmpty()) parts << sido.trimmed();
  if (!city.trimmed().isEmpty()) parts << city.trimmed();
  if (!dong.trimmed().isEmpty()) parts << dong.trimmed();
  if (parts.isEmpty()) return {};
  return QStringLiteral("full_nm:like:%1").arg(parts.join(QLatin1Char(' ')));
}

QUrl AdminBoundaryService::buildGetFeatureUrl(const QString& apiKey, const QString& sido,
                                              const QString& city, const QString& dong) {
  QUrl url(QStringLiteral("https://api.vworld.kr/req/data"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("service"), QStringLiteral("data"));
  q.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0"));
  q.addQueryItem(QStringLiteral("request"), QStringLiteral("GetFeature"));
  q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  q.addQueryItem(QStringLiteral("size"), QStringLiteral("10"));
  q.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("geometry"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("attribute"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:3857"));
  q.addQueryItem(QStringLiteral("data"), QString::fromLatin1(kDataset));
  const QString filter = attrFilter(sido, city, dong);
  if (!filter.isEmpty())
    q.addQueryItem(QStringLiteral("attrFilter"), filter);
  q.addQueryItem(QStringLiteral("key"), apiKey);
  url.setQuery(q);
  return url;
}

static QString normalizeCrsName(const QString& raw) {
  if (raw.contains(QLatin1String("5186"))) return QStringLiteral("EPSG:5186");
  if (raw.contains(QLatin1String("5187"))) return QStringLiteral("EPSG:5187");
  if (raw.contains(QLatin1String("4326"))) return QStringLiteral("EPSG:4326");
  if (raw.contains(QLatin1String("5179"))) return QStringLiteral("EPSG:5179");
  if (raw.contains(QLatin1String("3857")) || raw.contains(QLatin1String("900913")))
    return QStringLiteral("EPSG:3857");
  if (raw.startsWith(QLatin1String("EPSG:"))) return raw;
  return QStringLiteral("EPSG:3857");
}

static QString coordPair(const QJsonArray& xy) {
  if (xy.size() < 2) return {};
  const double x = xy.at(0).isString() ? xy.at(0).toString().toDouble() : xy.at(0).toDouble();
  const double y = xy.at(1).isString() ? xy.at(1).toString().toDouble() : xy.at(1).toDouble();
  return QString::number(x, 'f', 8) + QLatin1Char(' ') + QString::number(y, 'f', 8);
}

static QString ringWkt(const QJsonArray& ring) {
  QStringList pts;
  for (const QJsonValue& p : ring) {
    const QString pair = coordPair(p.toArray());
    if (!pair.isEmpty()) pts << pair;
  }
  return pts.join(QLatin1String(", "));
}

static QString polygonWkt(const QJsonArray& rings) {
  QStringList parts;
  for (const QJsonValue& r : rings) {
    const QString ring = ringWkt(r.toArray());
    if (!ring.isEmpty()) parts << QLatin1Char('(') + ring + QLatin1Char(')');
  }
  if (parts.isEmpty()) return {};
  return QStringLiteral("POLYGON(") + parts.join(QLatin1String(", ")) + QLatin1Char(')');
}

static QString geometryToWkt(const QJsonObject& geom) {
  const QString type = geom.value(QStringLiteral("type")).toString();
  const QJsonArray coords = geom.value(QStringLiteral("coordinates")).toArray();
  if (type.compare(QLatin1String("Polygon"), Qt::CaseInsensitive) == 0)
    return polygonWkt(coords);
  if (type.compare(QLatin1String("MultiPolygon"), Qt::CaseInsensitive) == 0) {
    QStringList polys;
    for (const QJsonValue& poly : coords) {
      const QString w = polygonWkt(poly.toArray());
      if (w.startsWith(QLatin1String("POLYGON(")))
        polys << w.mid(8, w.size() - 9);
    }
    if (polys.isEmpty()) return {};
    return QStringLiteral("MULTIPOLYGON(") + polys.join(QLatin1String(", ")) + QLatin1Char(')');
  }
  return {};
}

AdminBoundaryParse AdminBoundaryService::parseGetFeature(const QByteArray& body,
                                                         const QString& cityHint) {
  AdminBoundaryParse out;
  const QJsonObject root = QJsonDocument::fromJson(body).object();
  const QJsonObject resp = root.value(QStringLiteral("response")).toObject();
  const QString status = resp.value(QStringLiteral("status")).toString();
  if (status != QLatin1String("OK")) {
    const QString text = resp.value(QStringLiteral("error")).toObject()
                             .value(QStringLiteral("text")).toString();
    out.error = text.isEmpty() ? QStringLiteral("그 읍면동 경계를 찾지 못했습니다")
                               : QStringLiteral("그 읍면동 경계를 찾지 못했습니다 (%1)").arg(text);
    return out;
  }

  const QJsonObject fc = resp.value(QStringLiteral("result")).toObject()
                             .value(QStringLiteral("featureCollection")).toObject();
  const QString crsRaw = fc.value(QStringLiteral("crs")).toObject()
                             .value(QStringLiteral("properties")).toObject()
                             .value(QStringLiteral("name")).toString();
  out.crsAuthId = normalizeCrsName(crsRaw);

  const QJsonArray feats = fc.value(QStringLiteral("features")).toArray();
  QJsonObject chosen;
  for (const QJsonValue& v : feats) {
    const QJsonObject f = v.toObject();
    const QJsonObject props = f.value(QStringLiteral("properties")).toObject();
    const QString full = props.value(QStringLiteral("full_nm")).toString();
    if (!cityHint.trimmed().isEmpty() && !full.contains(cityHint.trimmed()))
      continue;
    chosen = f;
    break;
  }
  if (chosen.isEmpty() && !feats.isEmpty())
    chosen = feats.first().toObject();
  if (chosen.isEmpty()) {
    out.error = QStringLiteral("그 읍면동 경계를 찾지 못했습니다");
    return out;
  }

  const QJsonObject props = chosen.value(QStringLiteral("properties")).toObject();
  out.title = props.value(QStringLiteral("full_nm")).toString();
  if (out.title.isEmpty())
    out.title = props.value(QStringLiteral("emd_kor_nm")).toString();
  out.emdCode = props.value(QStringLiteral("emd_cd")).toString();
  if (out.emdCode.isEmpty())
    out.emdCode = QString::number(props.value(QStringLiteral("emd_cd")).toInt());
  out.wkt = geometryToWkt(chosen.value(QStringLiteral("geometry")).toObject());
  if (out.wkt.isEmpty()) {
    out.error = QStringLiteral("읍면동 경계 좌표가 비어 있습니다");
    return out;
  }
  out.ok = true;
  return out;
}

void AdminBoundaryService::fetchEmd(const QString& sido, const QString& city, const QString& dong) {
  if (m_pending) {
    emit failed(QStringLiteral("이전 행정구역 요청이 진행 중입니다"));
    return;
  }
  const QString key = VworldSettings::loadApiKey().trimmed();
  if (key.isEmpty()) {
    emit failed(QStringLiteral("VWorld 키가 없습니다. 설정에서 키를 넣은 뒤 다시 누르세요."));
    return;
  }
  if (city.trimmed().isEmpty() || dong.trimmed().isEmpty()) {
    emit failed(QStringLiteral("시·군·구와 읍·면·동을 모두 고르세요"));
    return;
  }

  m_pending = true;
  const QUrl url = buildGetFeatureUrl(key, sido, city, dong);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
  req.setRawHeader("Referer", "https://localhost");
  req.setRawHeader("Accept", "application/json");
  QNetworkReply* reply = m_nam.get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, sido, city]() {
    m_pending = false;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("행정구역을 받지 못했습니다: %1").arg(reply->errorString()));
      return;
    }
    QString hint = city.trimmed();
    if (!sido.trimmed().isEmpty())
      hint = sido.trimmed() + QLatin1Char(' ') + hint;
    const AdminBoundaryParse parsed = parseGetFeature(reply->readAll(), hint);
    if (!parsed.ok) {
      emit failed(parsed.error);
      return;
    }
    emit fetched(parsed);
  });
}
