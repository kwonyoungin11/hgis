#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <memory>

class QNetworkReply;

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
  AdminBoundaryService(std::unique_ptr<QNetworkAccessManager> network, int timeoutMs,
                       QObject* parent = nullptr);
  ~AdminBoundaryService() override;

  static QString attrFilter(const QString& sido, const QString& city, const QString& dong);
  static QUrl buildGetFeatureUrl(const QString& apiKey, const QString& sido, const QString& city,
                                 const QString& dong);
  static AdminBoundaryParse parseGetFeature(const QByteArray& body, const QString& cityHint = {});

  void fetchEmd(const QString& sido, const QString& city, const QString& dong);
  void cancel();

signals:
  void fetched(const AdminBoundaryParse& result);
  void failed(const QString& message);

private:
  std::unique_ptr<QNetworkAccessManager> m_nam;
  QPointer<QNetworkReply> m_reply;
  QTimer m_deadline;
  int m_timeoutMs;
  bool m_pending = false;
};
