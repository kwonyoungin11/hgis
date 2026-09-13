#pragma once

#include <QString>

// 포터블 폴더만 복사한 Windows PC에서 QGIS/PROJ/GDAL 경로를 자기 폴더에 묶는다.
// 개발 PC의 OSGeo 설치 경로(예: D:\OSGeo4W)에 의존하면 다른 컴퓨터에서
// EPSG:5186/5187/3857이 무효가 되어 위성·지적이 빈다.
struct KaPortablePaths {
  QString exeDir;
  QString qgisPrefix;
  QString projData;
  QString gdalData;
  QString caBundle;
  QString qtPlugins;
  QString qgisPlugins;
  bool looksBundled() const { return !qgisPrefix.isEmpty(); }
};

namespace KaPortableRuntime {
KaPortablePaths discover(const QString& exeDir);
bool applyEnvironment(const KaPortablePaths& paths);
// 설치된 OSGeo4W 로 도는 개발 빌드용. QtWebEngineProcess 같은 자식 프로세스가
// zlib.dll 등을 찾을 수 있도록 OSGeo4W 의 bin 폴더들을 PATH 앞에 붙인다.
bool prependOsgeoPath();
// QtWebEngine 자식 프로세스가 페이지를 실제로 열 수 있게 한다.
// 이 PC들에서는 Chromium 샌드박스가 뜨지 못해 모든 로드가 loadFinished(false) 로 끝난다.
// (2026-09-11 확인: --no-sandbox 만 있으면 되고 --disable-gpu 는 무관하다)
// 이미 QTWEBENGINE_CHROMIUM_FLAGS 가 설정돼 있으면 건드리지 않는다.
// QApplication 을 만들기 전에 불러야 한다.
bool applyWebEngineFlags();
void isolateUserState(const KaPortablePaths& paths);
bool bindProjSearchPaths(const QString& projDataDir);
bool koreaWorkAndWebCrsValid();
QString resolvedExeDir();
void setExeDirOverride(const QString& exeDir);
QString userConfigDir();
}
