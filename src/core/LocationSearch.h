#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>

struct LocationHit {
  QString title;
  QString detail;
  double lon = 0;
  double lat = 0;
  double west = 0, south = 0, east = 0, north = 0;
  bool hasBbox = false;
};

class LocationSearch : public QObject {
  Q_OBJECT
public:
  explicit LocationSearch(QObject* parent = nullptr);

  void search(const QString& query);
  static QString vworldApiKey();
  static void setVworldApiKey(const QString& key);

signals:
  void finished(const QVector<LocationHit>& hits);
  void failed(const QString& message);

private:
  void searchNominatim(const QString& query);
  void searchVworld(const QString& query);
  void handleNominatim(const QByteArray& body);
  void handleVworld(const QByteArray& body);

  QNetworkAccessManager m_nam;
  bool m_pending = false;
};
