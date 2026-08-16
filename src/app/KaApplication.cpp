#include "KaApplication.h"
#include "KaTheme.h"
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
#include <QToolButton>
#include <QDockWidget>
#include <QAction>
#include <QTreeView>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>
#include <functional>
#include <cstdlib>
#include "core/VworldSettings.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include <QTemporaryDir>

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
    for (auto* btn : tb->findChildren<QToolButton*>()) {
      if (!btn) continue;
      const QString t = btn->text().trimmed();
      if (!t.isEmpty()) toolbarTexts << t;
    }
    const QStringList need = {
      QStringLiteral("새조사"), QStringLiteral("열기"), QStringLiteral("저장"),
      QStringLiteral("그리기"), QStringLiteral("배경"), QStringLiteral("도면만들기")
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
  runBasemap(QStringLiteral("btn_vworld_sat"), QStringLiteral("위성"), [&](QString* e) {
    return LayerOps::addVworldSatelliteMap(QgsProject::instance(), canvas, key, e);
  });
  runBasemap(QStringLiteral("btn_vworld_cad"), QStringLiteral("지적"), [&](QString* e) {
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

  {
    QTemporaryDir tdir;
    QString err;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(
        tdir.isValid() ? tdir.path() : QDir::tempPath(), QStringLiteral("qa_survey"), &err,
        QStringLiteral("EPSG:5186"));
    const bool created = !gpkg.isEmpty() && QFile::exists(gpkg);
    all = step(QStringLiteral("factory_create_gpkg"), created, created ? gpkg : err) && all;
    if (created) {
      const bool loaded = w->openSurveyGpkg(gpkg);
      QCoreApplication::processEvents();
      const int domEmpty = w->domainLayerCount();
      all = step(QStringLiteral("new_survey_legend_empty"), loaded && domEmpty == 0,
                 QStringLiteral("loaded=%1 domain=%2 (must be 0 until user draws)")
                     .arg(loaded)
                     .arg(domEmpty)) &&
            all;
      QString ensErr;
      auto* fp = LayerOps::ensureDomainLayer(QgsProject::instance(), gpkg,
                                             QStringLiteral("feature_poly"),
                                             QStringLiteral("유구면"), &ensErr);
      QCoreApplication::processEvents();
      all = step(QStringLiteral("draw_tool_creates_layer"), fp != nullptr && w->domainLayerCount() >= 1,
                 fp ? QStringLiteral("feature_poly ready") : ensErr) &&
            all;
    }
  }

  if (canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), canvas, true);
    QCoreApplication::processEvents();
    all = step(QStringLiteral("canvas_has_layers"), canvas->layers().size() >= 1,
               QStringLiteral("canvas=%1 project=%2")
                   .arg(canvas->layers().size())
                   .arg(QgsProject::instance()->mapLayers().size())) &&
          all;
  }

  // Main toolbar + expandable sub-tools (draw/basemap/submit) after opening draw strip.
  if (tb) {
    auto hasText = [&](const QString& needle) {
      for (QAction* a : tb->actions()) {
        if (a && a->text().contains(needle)) return true;
      }
      for (auto* btn : tb->findChildren<QToolButton*>()) {
        if (btn && btn->text().contains(needle)) return true;
      }
      return false;
    };
    all = step(QStringLiteral("toolbar_new_action"), hasText(QStringLiteral("새조사"))) && all;
    all = step(QStringLiteral("toolbar_draw_toggle"), hasText(QStringLiteral("그리기"))) && all;
    all = step(QStringLiteral("toolbar_basemap_toggle"), hasText(QStringLiteral("배경"))) && all;
    all = step(QStringLiteral("toolbar_submit_toggle"), hasText(QStringLiteral("도면만들기"))) && all;
    all = step(QStringLiteral("toolbar_no_crs_peer"),
               !hasText(QStringLiteral("5186→")) && !hasText(QStringLiteral("5187→")) &&
                   !hasText(QStringLiteral("5186→5179")) && !hasText(QStringLiteral("5187→5179")),
               toolbarTexts.join(QLatin1Char(','))) &&
          all;
    bool sendInSubmitMenu = false;
    if (auto* btnSubmit = w->findChild<QToolButton*>(QStringLiteral("btnSubmit"))) {
      if (QMenu* sm = btnSubmit->menu()) {
        for (QAction* a : sm->actions()) {
          if (a && a->text().contains(QStringLiteral("보내기"))) {
            sendInSubmitMenu = true;
            break;
          }
        }
      }
    }
    all = step(QStringLiteral("submit_menu_send"), sendInSubmitMenu) && all;

    if (auto* btnDraw = w->findChild<QToolButton*>(QStringLiteral("btnDraw"))) {
      btnDraw->click();
      QCoreApplication::processEvents();
    }
    auto* sub = w->findChild<QToolBar*>(QStringLiteral("subToolbar"));
    auto subHas = [&](const QString& needle) {
      if (!sub || !sub->isVisible()) return false;
      for (QAction* a : sub->actions()) {
        if (a && a->text().contains(needle)) return true;
      }
      return false;
    };
    all = step(QStringLiteral("sub_draw_select"), subHas(QStringLiteral("선택"))) && all;
    all = step(QStringLiteral("sub_draw_snap"), subHas(QStringLiteral("자석"))) && all;
    all = step(QStringLiteral("sub_draw_area"), subHas(QStringLiteral("구역"))) && all;
    all = step(QStringLiteral("sub_draw_artifact"), subHas(QStringLiteral("유물"))) && all;
    all = step(QStringLiteral("sub_draw_attr"), subHas(QStringLiteral("속성"))) && all;
    all = step(QStringLiteral("sub_draw_merge"), subHas(QStringLiteral("묶기"))) && all;
  }
#else
  all = step(QStringLiteral("layerTree"), false, QStringLiteral("no qgis")) && all;
  all = step(QStringLiteral("mapCanvas"), false, QStringLiteral("no qgis")) && all;
#endif

  {
    auto* dock = w->findChild<QDockWidget*>(QStringLiteral("checkDock"));
    const bool hiddenAtStart = !dock || !dock->isVisible();
    all = step(QStringLiteral("checklist_dock_hidden_default"), hiddenAtStart,
               hiddenAtStart ? QStringLiteral("ok") : QStringLiteral("checkDock visible at start")) &&
          all;
    w->runChecklistPublic();
    QCoreApplication::processEvents();
    dock = w->findChild<QDockWidget*>(QStringLiteral("checkDock"));
    const bool shown = dock && dock->isVisible();
    all = step(QStringLiteral("checklist_dock_shows_on_run"), shown,
               shown ? QStringLiteral("shown") : QStringLiteral("dock missing/hidden")) &&
          all;

    const int errBeforeSeed = w->lastChecklistErrorCount();
    all = step(QStringLiteral("checklist_has_errors_before_seed"), errBeforeSeed > 0,
               QStringLiteral("errors=%1").arg(errBeforeSeed)) &&
          all;

    QString expErr;
    const QString blockedDir = QDir::temp().filePath(QStringLiteral("ka-hgis-export-block-qa"));
    const QString blocked = ExportService::exportSubmissionPackage(
        QgsProject::instance(), blockedDir, QStringLiteral("UTF-8"), QStringLiteral("qa"),
        /*blockOnError=*/true, /*hasChecklistErrors=*/errBeforeSeed > 0, &expErr);
    all = step(QStringLiteral("export_blocked_on_errors"), blocked.isEmpty() && errBeforeSeed > 0,
               blocked.isEmpty() ? expErr : QStringLiteral("exported unexpectedly: %1").arg(blocked)) &&
          all;

    const int seeded = w->seedDemoFieldData();
    QCoreApplication::processEvents();
    all = step(QStringLiteral("demo_seed_features"), seeded >= 3,
               QStringLiteral("seeded=%1").arg(seeded)) &&
          all;
    w->runChecklistPublic();
    QCoreApplication::processEvents();
    const int errAfterSeed = w->lastChecklistErrorCount();
    all = step(QStringLiteral("checklist_errors_drop_after_seed"),
               errAfterSeed >= 0 && errAfterSeed < errBeforeSeed,
               QStringLiteral("before=%1 after=%2").arg(errBeforeSeed).arg(errAfterSeed)) &&
          all;
  }

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
  bool demoSurvey = false;
  bool demoSeed = false;
  QString openGpkg;
  for (int i = 1; i < argc; ++i) {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == QLatin1String("--smoke-quit")) smokeQuit = true;
    if (a == QLatin1String("--qa-phase1")) qaPhase1 = true;
    if (a == QLatin1String("--demo-survey")) demoSurvey = true;
    if (a == QLatin1String("--demo-seed")) demoSeed = true;
    if (a.startsWith(QLatin1String("--open-gpkg=")))
      openGpkg = a.mid(QStringLiteral("--open-gpkg=").size());
    else if (a == QLatin1String("--open-gpkg") && i + 1 < argc)
      openGpkg = QString::fromLocal8Bit(argv[++i]);
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
                   QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3 QGIS"));
    const QString host = req->url().host();
    if (host.contains(QLatin1String("vworld.kr"), Qt::CaseInsensitive))
      req->setRawHeader("Referer", "https://localhost");
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
  KaTheme::apply(&app);

  if (VworldSettings::loadApiKey().isEmpty()) {
    const QByteArray envKey = qgetenv("VWORLD_API_KEY");
    if (!envKey.isEmpty())
      VworldSettings::saveApiKey(QString::fromUtf8(envKey).trimmed());
  }

  MainWindow w;
  w.show();
  if (demoSurvey && openGpkg.isEmpty()) {
    const QString dir = QDir::temp().filePath(QStringLiteral("ka-hgis-survey-verify"));
    QDir().mkpath(dir);
    QString err;
    openGpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("verify1"), &err,
                                                     QStringLiteral("EPSG:5186"));
    if (openGpkg.isEmpty())
      qWarning() << "demo-survey create failed:" << err;
  }
  if (!openGpkg.isEmpty()) {
    if (!w.openSurveyGpkg(openGpkg))
      qWarning() << "Failed to open survey gpkg:" << openGpkg;
    else {
      qInfo() << "Opened survey gpkg:" << openGpkg << "legend domain layers:" << w.domainLayerCount();
      if (demoSeed) {
        const int seeded = w.seedDemoFieldData();
        qInfo() << "Demo seed (explicit --demo-seed) features:" << seeded;
      }
    }
  }

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

