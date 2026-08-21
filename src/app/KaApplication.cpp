#include "KaApplication.h"
#include "KaCrashGuard.h"
#include "KaTheme.h"
#include "KaIcons.h"
#include "MainWindow.h"
#include <QElapsedTimer>
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
#include <QSplashScreen>
#include <QPainter>
#include <QLinearGradient>
#include <QPixmap>
#include <QFont>
#include <QPen>
#include <functional>
#include <cstdlib>
#include <QFileInfo>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "core/VworldSettings.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include <QTemporaryDir>

#if KA_HGIS_HAS_QGIS
#include <qgsapplication.h>
#include <qgsproviderregistry.h>
#include <qgsnetworkaccessmanager.h>
#include <qgssettings.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmessagelog.h>
#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <QNetworkRequest>
#endif

#ifdef Q_OS_WIN
#include <tlhelp32.h>

// 이미 같은 앱이 떠 있으면 그 창을 앞으로 가져온다(중복 실행 방지).
// 느린 부팅 중 아이콘을 다시 누르거나 두 번 실행하면 같은 조사 GPKG를
// 두 프로세스가 잡아 잠금 충돌·중복 다운로드가 나던 문제의 근본 대책.
static bool kaActivateExistingInstance() {
  CreateMutexW(nullptr, TRUE, L"Local\\ka-hgis-single-instance");
  if (GetLastError() != ERROR_ALREADY_EXISTS)
    return false;  // 첫 인스턴스 — 계속 부팅

  // 다른 ka-hgis.exe 프로세스들의 PID를 모은다.
  DWORD pids[64] = {};
  int pidCount = 0;
  const DWORD self = GetCurrentProcessId();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
      do {
        if (pe.th32ProcessID != self && _wcsicmp(pe.szExeFile, L"ka-hgis.exe") == 0 &&
            pidCount < 64)
          pids[pidCount++] = pe.th32ProcessID;
      } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
  }
  if (pidCount == 0) return true;  // 뮤텍스만 남은 상태 — 그래도 중복 부팅은 막는다

  struct Ctx {
    const DWORD* pids;
    int count;
    HWND found;
  } ctx{pids, pidCount, nullptr};
  EnumWindows(
      [](HWND h, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        if (!IsWindowVisible(h)) return TRUE;
        DWORD wpid = 0;
        GetWindowThreadProcessId(h, &wpid);
        for (int i = 0; i < c->count; ++i) {
          if (c->pids[i] == wpid && GetWindowTextLengthW(h) > 0) {
            c->found = h;
            return FALSE;
          }
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&ctx));
  if (ctx.found) {
    ShowWindow(ctx.found, SW_RESTORE);
    SetForegroundWindow(ctx.found);
  }
  return true;
}
#endif

// QCoreApplication 생성 전에도 exe 폴더를 알아야 해서 Win32로 직접 구한다.
static QString kaExeDir() {
#ifdef Q_OS_WIN
  wchar_t buf[4096];
  const DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
  if (n > 0 && n < 4096)
    return QFileInfo(QString::fromWCharArray(buf, int(n))).absolutePath();
#endif
  if (QCoreApplication::instance())
    return QCoreApplication::applicationDirPath();
  return {};
}

static void applyBundledRuntime() {
  const QDir app(kaExeDir());
  const QString qgis = app.filePath(QStringLiteral("apps/qgis-dev"));
  if (!QDir(qgis).exists())
    return;
  // Qt6/QGIS는 getenv 문자열을 UTF-8로 QString 변환한다. CP949로 넣으면 한글
  // 경로에서 prefix가 내부적으로 깨져 srs.db 등을 못 찾는다. UTF-8로 넣는다.
  qputenv("OSGEO4W_ROOT", app.absolutePath().toUtf8());
  // 포터블 구조가 감지되면 외부(run.bat 등)가 CP949로 넣은 값 대신 항상 덮어쓴다.
  qputenv("QGIS_PREFIX_PATH", qgis.toUtf8());
  // PROJ와 GDAL은 윈도우에서 경로 문자열을 UTF-8로 해석한다. CP949(encodeName)로
  // 넣으면 한글 폴더(예: "복사본", "바탕 화면")에서 proj.db를 못 찾아 좌표계 전체가
  // 죽고, 시작 시 위성·지적 자동 올리기가 실패한다. 반드시 UTF-8로 넣는다.
  const QString proj = app.filePath(QStringLiteral("share/proj"));
  if (QDir(proj).exists()) {
    qputenv("PROJ_DATA", proj.toUtf8());
    qputenv("PROJ_LIB", proj.toUtf8());
  }
  const QString gdal = app.filePath(QStringLiteral("apps/gdal-dev/share/gdal"));
  if (QDir(gdal).exists())
    qputenv("GDAL_DATA", gdal.toUtf8());
  const QString qtPlug = app.filePath(QStringLiteral("apps/Qt6/plugins"));
  if (QDir(qtPlug).exists()) {
    QCoreApplication::addLibraryPath(qtPlug);
    if (qgetenv("QT_PLUGIN_PATH").isEmpty())
      qputenv("QT_PLUGIN_PATH", QFile::encodeName(qtPlug));
  }
  const QString qgisPlug = QDir(qgis).filePath(QStringLiteral("qtplugins"));
  if (QDir(qgisPlug).exists())
    QCoreApplication::addLibraryPath(qgisPlug);
  QStringList prepend;
  const QStringList rels = {
      QString(),
      QStringLiteral("bin"),
      QStringLiteral("apps/qgis-dev/bin"),
      QStringLiteral("apps/Qt6/bin"),
      QStringLiteral("apps/gdal-dev/bin"),
      QStringLiteral("apps/pdal-dev/bin"),
  };
  for (const QString& rel : rels) {
    const QString p = rel.isEmpty() ? app.absolutePath() : app.filePath(rel);
    if (QDir(p).exists())
      prepend << QDir::toNativeSeparators(p);
  }
  const QString old = QString::fromLocal8Bit(qgetenv("PATH"));
  qputenv("PATH", (prepend.join(QLatin1Char(';')) + QLatin1Char(';') + old).toLocal8Bit());
}

QString KaApplication::resolvePrefixPath() {
  if (const char* e = std::getenv("QGIS_PREFIX_PATH")) {
    // applyBundledRuntime가 UTF-8로 넣는다(QGIS 내부 해석과 동일). 실패 시 로컬 인코딩 재시도.
    QString p = QString::fromUtf8(e);
    if (!QDir(p).exists())
      p = QString::fromLocal8Bit(e);
    if (QDir(p).exists())
      return p;
  }
  if (QCoreApplication::instance()) {
    const QString bundled =
        QCoreApplication::applicationDirPath() + QStringLiteral("/apps/qgis-dev");
    if (QDir(bundled).exists())
      return bundled;
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
    all = step(QStringLiteral("toolbar_measure_tape"), hasText(QStringLiteral("줄자"))) && all;
    all = step(QStringLiteral("toolbar_dem"), hasText(QStringLiteral("지형분석"))) && all;
    all = step(QStringLiteral("toolbar_trench_grid"), hasText(QStringLiteral("시굴격자"))) && all;
    all = step(QStringLiteral("map_grid_check"), hasText(QStringLiteral("좌표격자"))) && all;
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
  // 충돌 시 심볼 스택·미니덤프가 남도록 가장 먼저 설치한다.
  KaCrashGuard::install();
  QElapsedTimer bootTimer;
  bootTimer.start();

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

#ifdef Q_OS_WIN
  // 중복 실행 차단: 이미 떠 있으면 그 창을 앞으로 올리고 이 프로세스는 끝낸다.
  // 같은 조사 GPKG를 두 프로세스가 잡아 잠금 충돌·중복 다운로드가 나는 것을 막는다.
  // 자동 QA(--smoke-quit/--qa-phase1)는 항상 자기 프로세스로 끝까지 돌아야 하므로 제외.
  if (!smokeQuit && !qaPhase1 && kaActivateExistingInstance()) {
    KaCrashGuard::logLine(
        QStringLiteral("[boot] 이미 실행 중인 ka-hgis 창을 앞으로 올리고 종료합니다."));
    return 0;
  }
#endif

#if KA_HGIS_HAS_QGIS
  // PROJ/GDAL 환경은 QgsApplication이 첫 좌표계 컨텍스트를 만들기 전에 준비돼야 한다.
  applyBundledRuntime();
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
  KaCrashGuard::logLine(QStringLiteral("[boot] QGIS 초기화 %1 ms").arg(bootTimer.elapsed()));
  // QGIS 내부 경고(WMS 실패, 좌표계 문제 등)도 세션 로그로 남긴다.
  QObject::connect(
      QgsApplication::messageLog(),
      qOverload<const QString&, const QString&, Qgis::MessageLevel>(
          &QgsMessageLog::messageReceived),
      &app, [](const QString& message, const QString& tag, Qgis::MessageLevel level) {
        if (level == Qgis::MessageLevel::Warning || level == Qgis::MessageLevel::Critical)
          KaCrashGuard::logLine(QStringLiteral("[qgis/%1] %2").arg(tag, message));
      });
#else
  QApplication app(argc, argv);
  qWarning("Built without QGIS SDK (stub mode)");
#endif

  app.setApplicationName(QStringLiteral("ka-hgis"));
  app.setApplicationDisplayName(QStringLiteral("유적 HGIS"));
  // 배포 아이콘(딥블루 트라울+맵핀)이 있으면 그걸 쓰고, 없으면 그린 아이콘.
  {
    const QString icoPath = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("../data/theme/ka-hgis.ico"));
    const QString icoLocal = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("data/theme/ka-hgis.ico"));
    QIcon appIco;
    if (QFile::exists(icoLocal)) appIco = QIcon(icoLocal);
    else if (QFile::exists(icoPath)) appIco = QIcon(icoPath);
    app.setWindowIcon(appIco.isNull() ? KaIcons::appIcon() : appIco);
  }
  app.setOrganizationName(QStringLiteral("ka-hgis"));
  app.setApplicationVersion(QStringLiteral("0.3.0"));
  app.setStyle(QStringLiteral("Fusion"));
  KaTheme::apply(&app);
#if KA_HGIS_HAS_QGIS
  // 타일 요청이 순간 실패(VWorld 요청 제한·네트워크 흔들림)해도 프로바이더가 더
  // 재시도하도록 올린다(기본 3회). 위성지도가 반만 그려지는 주원인 완화.
  // 주의: 조직·앱 이름이 정해진 뒤에 써야 프로바이더가 읽는 저장소와 일치한다.
  {
    QgsSettings tileSettings;
    tileSettings.setValue(QStringLiteral("qgis/defaultTileMaxRetry"), 6);
    KaCrashGuard::logLine(QStringLiteral("[boot] 타일 재시도 한도 = %1")
                              .arg(tileSettings.value(QStringLiteral("qgis/defaultTileMaxRetry"), 3)
                                       .toInt()));
  }
#endif

  QSplashScreen* splash = nullptr;
  if (!smokeQuit) {
    QPixmap pm(640, 360);
    QLinearGradient g(0, 0, 0, pm.height());
    g.setColorAt(0.0, QColor(27, 36, 48));
    g.setColorAt(1.0, QColor(23, 90, 176));
    QPainter p(&pm);
    p.fillRect(pm.rect(), g);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(255, 255, 255, 48), 1));
    p.drawRect(pm.rect().adjusted(10, 10, -11, -11));
    p.setPen(Qt::white);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 28, QFont::Bold));
    p.drawText(pm.rect().adjusted(0, -28, 0, 0), Qt::AlignCenter, QStringLiteral("유적 HGIS"));
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11));
    p.setPen(QColor(232, 238, 245));
    p.drawText(pm.rect().adjusted(0, 40, 0, 0), Qt::AlignCenter,
               QStringLiteral("현장 조사를 불러오는 중…"));
    p.end();
    splash = new QSplashScreen(pm);
    splash->show();
    app.processEvents();
  }

  if (VworldSettings::loadApiKey().isEmpty()) {
    const QByteArray envKey = qgetenv("VWORLD_API_KEY");
    if (!envKey.isEmpty())
      VworldSettings::saveApiKey(QString::fromUtf8(envKey).trimmed());
  }

  MainWindow w;
  KaCrashGuard::logLine(QStringLiteral("[boot] 메인창 구성 %1 ms").arg(bootTimer.elapsed()));
  w.show();
  if (splash) {
    splash->finish(&w);
    delete splash;
    splash = nullptr;
  }
  KaCrashGuard::logLine(QStringLiteral("[boot] 창 표시 %1 ms").arg(bootTimer.elapsed()));
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

