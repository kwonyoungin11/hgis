#include <QtTest>
#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include <functional>

#include "core/AdminBoundaryService.h"
#include "core/LocationSearch.h"

namespace {
const QByteArray serviceError = R"({"response":{"status":"ERROR","error":{"code":"INCORRECT_KEY","text":"private server detail"}}})";
const QByteArray locationResult = R"([{"display_name":"test location","lat":"37.5","lon":"127.1","boundingbox":["37.4","37.6","127.0","127.2"]}])";
const QByteArray boundaryResult = R"({"response":{"status":"OK","result":{"featureCollection":{"features":[{"properties":{"full_nm":"test city dong","emd_cd":"123"},"geometry":{"type":"Polygon","coordinates":[[[1,1],[2,1],[2,2],[1,1]]]}}]}}}})";

class LocalServer : public QTcpServer {
public:
  LocalServer() {
    connect(this, &QTcpServer::newConnection, this, [this]() {
      while (hasPendingConnections()) {
        QTcpSocket* socket = nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
          QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if (!request.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
          socket->setProperty("handled", true);
          const QString path = QString::fromLatin1(request.split(' ').value(1));
          requests.push_back(path);
          if (handler) handler(socket, path);
        });
      }
    });
  }

  static void respond(QTcpSocket* socket, const QByteArray& body, int status = 200) {
    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\n"
                  "Content-Type: application/json\r\nConnection: close\r\nContent-Length: "
                  + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    socket->disconnectFromHost();
  }

  QStringList requests;
  std::function<void(QTcpSocket*, const QString&)> handler;
};

// Every request, including fallback, stays on localhost. Strip all query items so
// neither the user's API key nor search text can leave through the test server.
class LocalNetwork : public QNetworkAccessManager {
public:
  explicit LocalNetwork(quint16 port) : m_port(port) {
    setProxy(QNetworkProxy::NoProxy);
  }
  QList<int> transferTimeouts;
protected:
  QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                               QIODevice* outgoingData) override {
    transferTimeouts.push_back(request.transferTimeout());
    QNetworkRequest local(request);
    const QString path = request.url().host().contains(QLatin1String("nominatim"))
                           ? QStringLiteral("/nominatim") : QStringLiteral("/vworld");
    local.setUrl(QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_port).arg(path)));
    return QNetworkAccessManager::createRequest(operation, local, outgoingData);
  }
private:
  quint16 m_port;
};
}

class NetworkServicesTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("ka-network-test-")
                                        + QUuid::createUuid().toString(QUuid::Id128));
    m_settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QVERIFY(QDir().mkpath(m_settingsDir));
    QSettings settings(QDir(m_settingsDir).filePath(QStringLiteral("ka-hgis-vworld.ini")),
                       QSettings::IniFormat);
    settings.setValue(QStringLiteral("VWorld/ApiKey"), QStringLiteral("local-test-key"));
    settings.sync();
    m_oldKey = qgetenv("VWORLD_API_KEY");
    m_hadKey = qEnvironmentVariableIsSet("VWORLD_API_KEY");
    qputenv("VWORLD_API_KEY", "local-test-key");
  }

  void cleanupTestCase() {
    if (m_hadKey) qputenv("VWORLD_API_KEY", m_oldKey);
    else qunsetenv("VWORLD_API_KEY");
    QFile::remove(QDir(m_settingsDir).filePath(QStringLiteral("ka-hgis-vworld.ini")));
    QDir().rmdir(m_settingsDir);
  }

  void locationDeadlineReleasesPendingAndAllowsRetry() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto network = std::make_unique<LocalNetwork>(server.serverPort());
    LocalNetwork* observed = network.get();
    LocationSearch service(std::move(network), 400);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("first"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("다시")));
    QCOMPARE(finished.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, path == QLatin1String("/vworld") ? serviceError : locationResult);
    };
    service.search(QStringLiteral("retry"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(failed.size(), 1);
    for (int timeout : observed->transferTimeouts) QCOMPARE(timeout, 400);
  }

  void fallbackKeepsPendingUntilItsResult() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QPointer<QTcpSocket> fallback;
    server.handler = [&fallback](QTcpSocket* socket, const QString& path) {
      if (path == QLatin1String("/vworld")) LocalServer::respond(socket, serviceError);
      else fallback = socket;
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("first"));
    QTRY_VERIFY_WITH_TIMEOUT(fallback, 3000);
    service.search(QStringLiteral("conflicting search"));
    QCOMPARE(failed.size(), 1);
    QCOMPARE(server.requests.size(), 2);
    LocalServer::respond(fallback, locationResult);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(server.requests.size(), 2);
    const QVector<LocationHit> hits = qvariant_cast<QVector<LocationHit>>(finished.first().first());
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.first().lon, 127.1);
  }

  void fallbackFailureIsNotSuccess() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, serviceError, path == QLatin1String("/vworld") ? 503 : 200);
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QVERIFY(!failed.first().first().toString().contains(QStringLiteral("private")));
    service.search(QStringLiteral("retry"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 3000);
    QCOMPARE(server.requests.size(), 4);
  }

  void deadlineStopsFallbackEvenWhileBytesKeepArriving() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int receivedRequests = 0;
    server.handler = [&receivedRequests](QTcpSocket* socket, const QString& path) {
      ++receivedRequests;
      if (path == QLatin1String("/vworld")) {
        LocalServer::respond(socket, serviceError);
        return;
      }
      socket->write("HTTP/1.1 200 OK\r\nContent-Length: 1000000\r\n\r\n[");
      auto* trickle = new QTimer(socket);
      QObject::connect(trickle, &QTimer::timeout, socket, [socket]() {
        if (socket->state() == QAbstractSocket::ConnectedState) socket->write(" ");
      });
      trickle->start(20);
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 500);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(receivedRequests, 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("시간이 초과")));
  }

  void invalidCoordinatesCannotBecomeSuccessfulLocation() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.handler = [](QTcpSocket* socket, const QString& path) {
      LocalServer::respond(socket, path == QLatin1String("/vworld") ? serviceError
        : QByteArray(R"([{"display_name":"invalid","lat":"not-a-number","lon":"127"}])"));
    };
    LocationSearch service(std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(&service, &LocationSearch::failed);
    QSignalSpy finished(&service, &LocationSearch::finished);
    service.search(QStringLiteral("test"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(finished.size(), 0);
  }

  void locationCancelAndDestructionDisconnectReplies() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto service = std::make_unique<LocationSearch>(
      std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(service.get(), &LocationSearch::failed);
    QSignalSpy finished(service.get(), &LocationSearch::finished);
    service->search(QStringLiteral("cancel"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    service->cancel();
    service->search(QStringLiteral("new request"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 3000);
    service.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(failed.size(), 0);
    QCOMPARE(finished.size(), 0);
  }

  void boundaryDeadlineAndServiceErrorAllowRetry() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    AdminBoundaryService service(std::make_unique<LocalNetwork>(server.serverPort()), 400);
    QSignalSpy failed(&service, &AdminBoundaryService::failed);
    QSignalSpy fetched(&service, &AdminBoundaryService::fetched);
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
    QCOMPARE(fetched.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, serviceError); };
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 3000);
    QVERIFY(failed.last().first().toString().contains(QStringLiteral("API 키")));
    QVERIFY(!failed.last().first().toString().contains(QStringLiteral("private")));
    QCOMPARE(fetched.size(), 0);
    server.handler = [](QTcpSocket* socket, const QString&) { LocalServer::respond(socket, boundaryResult); };
    service.fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(fetched.size(), 1, 3000);
    QCOMPARE(failed.size(), 2);
  }

  void boundaryCancelAndDestructionDisconnectReplies() {
    LocalServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    auto service = std::make_unique<AdminBoundaryService>(
      std::make_unique<LocalNetwork>(server.serverPort()), 3000);
    QSignalSpy failed(service.get(), &AdminBoundaryService::failed);
    QSignalSpy fetched(service.get(), &AdminBoundaryService::fetched);
    service->fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 3000);
    service->cancel();
    service->fetchEmd(QStringLiteral("test"), QStringLiteral("city"), QStringLiteral("dong"));
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 3000);
    service.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(failed.size(), 0);
    QCOMPARE(fetched.size(), 0);
  }

private:
  QString m_settingsDir;
  QByteArray m_oldKey;
  bool m_hadKey = false;
};

QTEST_GUILESS_MAIN(NetworkServicesTest)
#include "test_network_services.moc"
