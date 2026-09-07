#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QNetworkAccessManager>

struct AdminBoundaryParse {
  bool ok = false;
  QString error;
  QString title;
  QString emdCode;
  QString wkt;
  QString crsAuthId;
};

class AdminBoundaryService : public QObject {
  Q_OBJECT
public:
  static constexpr const char* kDataset = "LT_C_ADEMD_INFO";

  explicit AdminBoundaryService(QObject* parent = nullptr);

  static QString attrFilter(const QString& sido, const QString& city, const QString& dong);
  static QUrl buildGetFeatureUrl(const QString& apiKey, const QString& sido, const QString& city,
                                 const QString& dong);
  static AdminBoundaryParse parseGetFeature(const QByteArray& body, const QString& cityHint = {});

  void fetchEmd(const QString& sido, const QString& city, const QString& dong);

signals:
  void fetched(const AdminBoundaryParse& result);
  void failed(const QString& message);

private:
  QNetworkAccessManager m_nam;
  bool m_pending = false;
};
