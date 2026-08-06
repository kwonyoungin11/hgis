#include "KaApplication.h"
#include "MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QCoreApplication>
#include <cstdlib>

#if KA_HGIS_HAS_QGIS
#include <qgsapplication.h>
#include <qgsproviderregistry.h>
#endif

QString KaApplication::resolvePrefixPath() {
  if (const char* e = std::getenv("QGIS_PREFIX_PATH")) {
    return QString::fromLocal8Bit(e);
  }
  if (const char* o = std::getenv("OSGEO4W_ROOT")) {
    const QString root = QString::fromLocal8Bit(o);
    const QStringList cands = {
      root + "/apps/qgis-ltr-dev",
      root + "/apps/qgis-ltr",
      root + "/apps/qgis-dev",
      root + "/apps/qgis"
    };
    for (const QString& c : cands) {
      if (QDir(c).exists()) return c;
    }
  }
  return QString();
}

int KaApplication::run(int argc, char** argv) {
  bool smokeQuit = false;
  for (int i = 1; i < argc; ++i) {
    if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--smoke-quit"))
      smokeQuit = true;
  }

#if KA_HGIS_HAS_QGIS
  QgsApplication app(argc, argv, true);
  const QString prefix = resolvePrefixPath();
  if (prefix.isEmpty()) {
    qCritical("QGIS_PREFIX_PATH/OSGEO4W_ROOT not set or qgis apps dir missing");
    return 2;
  }
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  qInfo() << "QGIS prefix:" << prefix;
  qInfo() << "Providers:" << QgsProviderRegistry::instance()->providerList();
#else
  QApplication app(argc, argv);
  qWarning("Built without QGIS SDK (stub mode)");
#endif

  app.setApplicationName(QStringLiteral("ka-hgis"));
  app.setApplicationDisplayName(QStringLiteral("고고학 전용 HGIS"));

  MainWindow w;
  w.show();

  if (smokeQuit) {
    QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
  }

  const int code = app.exec();
#if KA_HGIS_HAS_QGIS
  QgsApplication::exitQgis();
#endif
  return code;
}

