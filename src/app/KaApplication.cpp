#include "KaApplication.h"
#include "MainWindow.h"
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QCoreApplication>
#include <QToolBar>
#include <QAction>
#include <QTreeView>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>
#include <functional>
#include <cstdlib>
#include "core/VworldSettings.h"
#include "core/LayerOps.h"

#if KA_HGIS_HAS_QGIS
#include <qgsapplication.h>
#include <qgsproviderregistry.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <QNetworkRequest>
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

#if KA_HGIS_HAS_QGIS
static QStringList qaLayerNames() {
  QStringList names;
  for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
    if (l) names << l->name();
  }
  return names;
}

static bool qaHasLayerLike(const QString& base) {
  for (const QString& n : qaLayerNames()) {
    if (n == base || n.startsWith(base + QLatin1String(" [")))
      return true;
  }
  return false;
}
#endif

static int writePhase1Qa(MainWindow* w, const QString& outPath) {
  QJsonArray steps;
  auto step = [&](const QString& name, bool ok, const QString& detail = {}) {
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("ok"), ok);
    if (!detail.isEmpty()) o.insert(QStringLiteral("detail"), detail);
    steps.append(o);
    return ok;
  };

  bool all = true;
  all = step(QStringLiteral("main_window"), w != nullptr) && all;
  if (!w) {
    QJsonObject root;
    root.insert(QStringLiteral("ok"), false);
    root.insert(QStringLiteral("steps"), steps);
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QFile f(outPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return 3;
  }

  auto* tb = w->findChild<QToolBar*>(QStringLiteral("mainToolbar"));
  all = step(QStringLiteral("mainToolbar"), tb != nullptr) && all;
  QStringList toolbarTexts;
  if (tb) {
    for (QAction* a : tb->actions()) {
      if (a && !a->isSeparator() && !a->text().isEmpty()) {
        toolbarTexts << a->text();
        all = step(QStringLiteral("toolbar_enabled:") + a->text(), a->isEnabled(),
                   a->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled")) && all;
      }
    }
    const QStringList need = {
      QStringLiteral("새조사"), QStringLiteral("그리기"), QStringLiteral("배경"),
      QStringLiteral("제출"), QStringLiteral("편집저장"), QStringLiteral("삭제"),
      QStringLiteral("열기"), QStringLiteral("저장")
    };
    QStringList missing;
    for (const QString& n : need) {
      bool found = false;
      for (const QString& t : toolbarTexts) {
        if (t.contains(n)) { found = true; break; }
      }
      if (!found) missing << n;
    }
    all = step(QStringLiteral("toolbar_actions"), missing.isEmpty(),
               missing.isEmpty() ? toolbarTexts.join(QLatin1Char(','))
                                 : QStringLiteral("missing:") + missing.join(QLatin1Char(','))) && all;
  }

  QStringList menuTexts;
  if (auto* mb = w->menuBar()) {
    for (QAction* top : mb->actions()) {
      if (!top) continue;
      menuTexts << top->text();
      if (QMenu* m = top->menu()) {
        for (QAction* a : m->actions()) {
          if (a && !a->isSeparator() && !a->text().isEmpty())
            menuTexts << (top->text() + QLatin1Char('/') + a->text());
        }
      }
    }
  }
  all = step(QStringLiteral("menus_present"), menuTexts.size() >= 10,
             menuTexts.join(QLatin1Char('|'))) && all;

  all = step(QStringLiteral("fileBrowser"),
             w->findChild<QTreeView*>(QStringLiteral("fileBrowser")) != nullptr) && all;
  all = step(QStringLiteral("btnBrowseDocs"),
             w->findChild<QWidget*>(QStringLiteral("btnBrowseDocs")) != nullptr) && all;
  all = step(QStringLiteral("btnBrowseFolder"),
             w->findChild<QWidget*>(QStringLiteral("btnBrowseFolder")) != nullptr) && all;

#if KA_HGIS_HAS_QGIS
  all = step(QStringLiteral("layerTree"),
             w->findChild<QgsLayerTreeView*>(QStringLiteral("layerTree")) != nullptr) && all;
  all = step(QStringLiteral("mapCanvas"),
             w->findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas")) != nullptr) && all;
  const bool hasWms = QgsProviderRegistry::instance()->providerList().contains(QStringLiteral("wms"));
  all = step(QStringLiteral("wms_provider"), hasWms) && all;

  auto* canvas = w->findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
  const QString key = VworldSettings::loadApiKey();
  all = step(QStringLiteral("vworld_key_present"), !key.isEmpty(),
             key.isEmpty() ? QStringLiteral("empty") : QStringLiteral("len=%1").arg(key.size())) &&
        all;

  auto runBasemap = [&](const QString& label, const QString& hint,
                        const std::function<bool(QString*)>& fn) {
    QString err;
    const int before = QgsProject::instance()->mapLayers().size();
    const bool okAdd = fn(&err);
    QCoreApplication::processEvents();
    bool has = qaHasLayerLike(hint);
    if (!has) {
      for (const QString& n : qaLayerNames()) {
        if (n.contains(hint, Qt::CaseInsensitive)) {
          has = true;
          break;
        }
      }
    }
    const int after = QgsProject::instance()->mapLayers().size();
    all = step(label, okAdd && has,
               QStringLiteral("okAdd=%1 has=%2 before=%3 after=%4 err=%5 layers=[%6]")
                   .arg(okAdd)
                   .arg(has)
                   .arg(before)
                   .arg(after)
                   .arg(err)
                   .arg(qaLayerNames().join(QLatin1Char(',')))) &&
          all;
  };

  runBasemap(QStringLiteral("btn_osm"), QStringLiteral("OSM"), [&](QString* e) {
    return LayerOps::addOsmBasemap(QgsProject::instance(), canvas, e);
  });
  runBasemap(QStringLiteral("btn_google"), QStringLiteral("Google"), [&](QString* e) {
    return LayerOps::addKoreaBasemap(QgsProject::instance(), canvas,
                                     LayerOps::KoreaBasemap::GoogleSatellite, e);
  });
  runBasemap(QStringLiteral("btn_vworld_base"), QStringLiteral("VWorld"), [&](QString* e) {
    return LayerOps::addVworldBaseMap(QgsProject::instance(), canvas, key, e);
  });
  runBasemap(QStringLiteral("btn_vworld_sat"), QStringLiteral("VWorld"), [&](QString* e) {
    return LayerOps::addVworldSatelliteMap(QgsProject::instance(), canvas, key, e);
  });
  runBasemap(QStringLiteral("btn_vworld_cad"), QStringLiteral("VWorld"), [&](QString* e) {
    return LayerOps::addVworldCadastralMap(QgsProject::instance(), canvas, key, e);
  });

  // Tighten sat/cad name checks separately.
  all = step(QStringLiteral("layer_sat_named"), qaHasLayerLike(QStringLiteral("VWorld 위성")) ||
                                                   [&]() {
                                                     for (const QString& n : qaLayerNames())
                                                       if (n.contains(QStringLiteral("위성"))) return true;
                                                     return false;
                                                   }(),
             qaLayerNames().join(QLatin1Char(','))) &&
        all;
  all = step(QStringLiteral("layer_cad_named"), qaHasLayerLike(QStringLiteral("VWorld 지적도")) ||
                                                   [&]() {
                                                     for (const QString& n : qaLayerNames())
                                                       if (n.contains(QStringLiteral("지적"))) return true;
                                                     return false;
                                                   }(),
             qaLayerNames().join(QLatin1Char(','))) &&
        all;

  all = step(QStringLiteral("layers_final_count"),
             QgsProject::instance()->mapLayers().size() >= 3,
             qaLayerNames().join(QLatin1Char(','))) &&
        all;

  if (canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), canvas, true);
    QCoreApplication::processEvents();
    all = step(QStringLiteral("canvas_has_layers"), canvas->layers().size() >= 1,
               QStringLiteral("canvas=%1 project=%2")
                   .arg(canvas->layers().size())
                   .arg(QgsProject::instance()->mapLayers().size())) &&
          all;
  }

  // Toolbar actions that map to basemap slots: trigger via QAction text match (no dialogs).
  if (tb) {
    auto triggerIf = [&](const QString& needle, const QString& label) {
      for (QAction* a : tb->actions()) {
        if (a && a->text().contains(needle)) {
          all = step(label, a->isEnabled(), a->text()) && all;
          return;
        }
      }
      all = step(label, false, QStringLiteral("action missing")) && all;
    };
    triggerIf(QStringLiteral("위성"), QStringLiteral("toolbar_sat_action"));
    triggerIf(QStringLiteral("지적"), QStringLiteral("toolbar_cad_action"));
    triggerIf(QStringLiteral("새조사"), QStringLiteral("toolbar_new_action"));
    triggerIf(QStringLiteral("폴리곤"), QStringLiteral("toolbar_poly_action"));
    triggerIf(QStringLiteral("선"), QStringLiteral("toolbar_line_action"));
    triggerIf(QStringLiteral("구역"), QStringLiteral("toolbar_area_action"));
    triggerIf(QStringLiteral("GPS"), QStringLiteral("toolbar_gps_action"));
    triggerIf(QStringLiteral("조판PDF"), QStringLiteral("toolbar_pdf_action"));
    triggerIf(QStringLiteral("5179"), QStringLiteral("toolbar_5179_action"));
    triggerIf(QStringLiteral("SHP"), QStringLiteral("toolbar_shp_action"));
    triggerIf(QStringLiteral("삭제"), QStringLiteral("toolbar_del_action"));
    triggerIf(QStringLiteral("편집저장"), QStringLiteral("toolbar_saveedit_action"));
    triggerIf(QStringLiteral("그리기종료"), QStringLiteral("toolbar_stopedit_action"));
  }
#else
  all = step(QStringLiteral("layerTree"), false, QStringLiteral("no qgis")) && all;
  all = step(QStringLiteral("mapCanvas"), false, QStringLiteral("no qgis")) && all;
#endif

  const bool checkVisible = [&]() {
    auto* cv = w->findChild<QWidget*>(QStringLiteral("checkView"));
    return cv && cv->isVisible();
  }();
  all = step(QStringLiteral("checklist_not_primary"), !checkVisible,
             checkVisible ? QStringLiteral("checkView visible") : QStringLiteral("ok")) && all;

  QJsonObject root;
  root.insert(QStringLiteral("ok"), all);
  root.insert(QStringLiteral("steps"), steps);
  QDir().mkpath(QFileInfo(outPath).absolutePath());
  QFile f(outPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return 4;
  f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return all ? 0 : 5;
}

int KaApplication::run(int argc, char** argv) {
  bool smokeQuit = false;
  bool qaPhase1 = false;
  for (int i = 1; i < argc; ++i) {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == QLatin1String("--smoke-quit")) smokeQuit = true;
    if (a == QLatin1String("--qa-phase1")) qaPhase1 = true;
  }

#if KA_HGIS_HAS_QGIS
  QgsApplication app(argc, argv, true);
  const QString prefix = resolvePrefixPath();
  if (prefix.isEmpty()) {
    qCritical("QGIS_PREFIX_PATH/OSGEO4W_ROOT not set or qgis apps dir missing");
    return 2;
  }
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::setPkgDataPath(prefix);
  QgsApplication::initQgis();
  QgsNetworkAccessManager::instance()->setupDefaultProxyAndCache();
  QgsNetworkAccessManager::setRequestPreprocessor([](QNetworkRequest* req) {
    if (!req) return;
    req->setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("ka-hgis/0.3 (QGIS-based archaeology field HGIS)"));
  });
  qInfo() << "QGIS prefix:" << prefix;
  qInfo() << "Providers:" << QgsProviderRegistry::instance()->providerList();
  if (!QgsProviderRegistry::instance()->providerList().contains(QStringLiteral("wms"))) {
    qCritical("WMS/XYZ provider missing — basemap tiles will not load");
  }
#else
  QApplication app(argc, argv);
  qWarning("Built without QGIS SDK (stub mode)");
#endif

  app.setApplicationName(QStringLiteral("ka-hgis"));
  app.setApplicationDisplayName(QStringLiteral("고고학 전용 HGIS"));
  app.setOrganizationName(QStringLiteral("ka-hgis"));
  app.setApplicationVersion(QStringLiteral("0.3.0"));
  app.setStyle(QStringLiteral("Fusion"));
  {
    QPalette light;
    light.setColor(QPalette::Window, QColor(232, 241, 251));
    light.setColor(QPalette::WindowText, QColor(15, 23, 42));
    light.setColor(QPalette::Base, Qt::white);
    light.setColor(QPalette::AlternateBase, QColor(247, 251, 255));
    light.setColor(QPalette::Text, QColor(15, 23, 42));
    light.setColor(QPalette::Button, QColor(232, 241, 251));
    light.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    light.setColor(QPalette::BrightText, QColor(15, 23, 42));
    light.setColor(QPalette::ToolTipBase, QColor(255, 251, 235));
    light.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    light.setColor(QPalette::Highlight, QColor(43, 108, 176));
    light.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(light);
  }

  if (VworldSettings::loadApiKey().isEmpty()) {
    const QByteArray envKey = qgetenv("VWORLD_API_KEY");
    if (!envKey.isEmpty())
      VworldSettings::saveApiKey(QString::fromUtf8(envKey).trimmed());
  }

  MainWindow w;
  w.show();

  int qaCode = 0;
  if (qaPhase1) {
    const QString out = QDir(QCoreApplication::applicationDirPath())
                            .filePath(QStringLiteral("../qa/phase1-e2e.json"));
    const QString out2 = QStringLiteral("build/qa/phase1-e2e.json");
    QDir().mkpath(QStringLiteral("build/qa"));
    qaCode = writePhase1Qa(&w, out2);
    if (qaCode != 0)
      writePhase1Qa(&w, out);
    QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
  } else if (smokeQuit) {
    QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
  }

  const int code = app.exec();
#if KA_HGIS_HAS_QGIS
  QgsApplication::exitQgis();
#endif
  if (qaPhase1) return qaCode;
  return code;
}

