#include "TilePackService.h"

#include <cmath>
#include <vector>

#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <cpl_conv.h>
#include <cpl_string.h>
#include <gdal.h>
#include <gdal_utils.h>

namespace TilePackService {

double webMercatorHalfWorld() { return 20037508.342789244; }

double resolutionAtZoom(int z) {
  if (z < 0) z = 0;
  // 한 변 2^z 타일 × 256 px가 세계를 덮는다.
  return (webMercatorHalfWorld() * 2.0) / (256.0 * std::pow(2.0, z));
}

qint64 tileCount(double minX, double minY, double maxX, double maxY, int minZoom, int maxZoom) {
  if (minZoom > maxZoom) std::swap(minZoom, maxZoom);
  if (minZoom < 0) minZoom = 0;
  if (maxZoom > 22) maxZoom = 22;
  if (!(maxX > minX) || !(maxY > minY)) return 0;

  const double half = webMercatorHalfWorld();
  qint64 total = 0;
  for (int z = minZoom; z <= maxZoom; ++z) {
    const double span = (half * 2.0) / std::pow(2.0, z);  // 타일 한 변(m)
    // XYZ는 위쪽이 원점이라 y는 위에서부터 센다.
    const qint64 c0 = static_cast<qint64>(std::floor((minX + half) / span));
    const qint64 c1 = static_cast<qint64>(std::floor((maxX + half) / span));
    const qint64 r0 = static_cast<qint64>(std::floor((half - maxY) / span));
    const qint64 r1 = static_cast<qint64>(std::floor((half - minY) / span));
    total += (c1 - c0 + 1) * (r1 - r0 + 1);
  }
  return total;
}

QString serviceXml(const Options& opt) {
  const double half = webMercatorHalfWorld();
  // GDAL TMS 미니드라이버는 ${z} ${x} ${y}를 쓴다. QGIS 표기를 바꿔 준다.
  QString url = opt.urlTemplate;
  url.replace(QStringLiteral("{z}"), QStringLiteral("${z}"));
  url.replace(QStringLiteral("{x}"), QStringLiteral("${x}"));
  url.replace(QStringLiteral("{y}"), QStringLiteral("${y}"));
  url.replace(QLatin1Char('&'), QLatin1String("&amp;"));

  QString xml;
  xml += QStringLiteral("<GDAL_WMS>");
  xml += QStringLiteral("<Service name=\"TMS\"><ServerUrl>%1</ServerUrl></Service>").arg(url);
  xml += QStringLiteral("<DataWindow>");
  xml += QStringLiteral("<UpperLeftX>%1</UpperLeftX>").arg(-half, 0, 'f', 6);
  xml += QStringLiteral("<UpperLeftY>%1</UpperLeftY>").arg(half, 0, 'f', 6);
  xml += QStringLiteral("<LowerRightX>%1</LowerRightX>").arg(half, 0, 'f', 6);
  xml += QStringLiteral("<LowerRightY>%1</LowerRightY>").arg(-half, 0, 'f', 6);
  xml += QStringLiteral("<TileLevel>%1</TileLevel>").arg(opt.maxZoom);
  xml += QStringLiteral("<TileCountX>1</TileCountX><TileCountY>1</TileCountY>");
  // XYZ(구글식)는 위가 원점. TMS 기본은 아래라 반드시 지정해야 남북이 안 뒤집힌다.
  xml += QStringLiteral("<YOrigin>top</YOrigin>");
  xml += QStringLiteral("</DataWindow>");
  xml += QStringLiteral("<Projection>EPSG:3857</Projection>");
  xml += QStringLiteral("<BlockSizeX>256</BlockSizeX><BlockSizeY>256</BlockSizeY>");
  xml += QStringLiteral("<BandsCount>%1</BandsCount>").arg(opt.bandCount);
  // 타일 서버가 한두 장 실패해도 전체가 깨지지 않게 한다.
  xml += QStringLiteral("<ZeroBlockHttpCodes>204,404,403,500,502,503,504</ZeroBlockHttpCodes>");
  xml += QStringLiteral("<ZeroBlockOnServerException>true</ZeroBlockOnServerException>");
  xml += QStringLiteral("<Timeout>30</Timeout><MaxConnections>4</MaxConnections>");
  if (!opt.referer.isEmpty())
    xml += QStringLiteral("<Referer>%1</Referer>").arg(opt.referer);
  xml += QStringLiteral("<Cache/>");
  xml += QStringLiteral("</GDAL_WMS>");
  return xml;
}

bool build(const Options& opt, double minX, double minY, double maxX, double maxY,
           const QString& outPath, QString* errorOut) {
  auto fail = [&](const QString& m) {
    if (errorOut) *errorOut = m;
    return false;
  };
  if (opt.urlTemplate.isEmpty()) return fail(QStringLiteral("타일 주소가 비었습니다."));
  if (!(maxX > minX) || !(maxY > minY))
    return fail(QStringLiteral("범위가 비었습니다. 조사구역을 먼저 그리세요."));
  if (outPath.isEmpty()) return fail(QStringLiteral("저장 경로가 비었습니다."));

  GDALAllRegister();
  if (!GDALGetDriverByName("MBTiles"))
    return fail(QStringLiteral("이 GDAL에는 MBTiles 드라이버가 없습니다."));

  const QByteArray xml = serviceXml(opt).toUtf8();
  GDALDatasetH src = GDALOpen(xml.constData(), GA_ReadOnly);
  if (!src)
    return fail(QStringLiteral("타일 서비스를 열지 못했습니다: %1")
                    .arg(QString::fromUtf8(CPLGetLastErrorMsg())));

  // 최대 줌의 해상도에 맞춰 출력 크기를 잡는다. 그래야 그 줌의 타일을 그대로 받는다.
  const double res = resolutionAtZoom(opt.maxZoom);
  const int outW = qMax(1, static_cast<int>(std::lround((maxX - minX) / res)));
  const int outH = qMax(1, static_cast<int>(std::lround((maxY - minY) / res)));

  QDir().mkpath(QFileInfo(outPath).absolutePath());
  if (QFile::exists(outPath)) QFile::remove(outPath);

  char** argv = nullptr;
  argv = CSLAddString(argv, "-of");
  argv = CSLAddString(argv, "MBTiles");
  argv = CSLAddString(argv, "-projwin");
  argv = CSLAddString(argv, QString::number(minX, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(maxY, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(maxX, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(minY, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, "-outsize");
  argv = CSLAddString(argv, QByteArray::number(outW).constData());
  argv = CSLAddString(argv, QByteArray::number(outH).constData());
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, opt.jpeg ? "TILE_FORMAT=JPEG" : "TILE_FORMAT=PNG");
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, QStringLiteral("MINZOOM=%1").arg(opt.minZoom).toUtf8().constData());
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, QStringLiteral("MAXZOOM=%1").arg(opt.maxZoom).toUtf8().constData());

  GDALTranslateOptions* tOpt = GDALTranslateOptionsNew(argv, nullptr);
  CSLDestroy(argv);
  if (!tOpt) {
    GDALClose(src);
    return fail(QStringLiteral("내려받기 설정을 만들지 못했습니다."));
  }

  int usageError = 0;
  GDALDatasetH out =
      GDALTranslate(outPath.toUtf8().constData(), src, tOpt, &usageError);
  GDALTranslateOptionsFree(tOpt);
  GDALClose(src);
  if (!out) {
    return fail(QStringLiteral("타일을 받지 못했습니다: %1")
                    .arg(QString::fromUtf8(CPLGetLastErrorMsg())));
  }

  // 낮은 줌(멀리 볼 때)은 오버뷰로 채운다. 없으면 줌아웃에서 빈 화면이 된다.
  const int levels = qMax(0, opt.maxZoom - opt.minZoom);
  if (levels > 0) {
    std::vector<int> factors;
    for (int i = 1; i <= levels; ++i) factors.push_back(1 << i);
    GDALBuildOverviews(out, opt.jpeg ? "AVERAGE" : "NEAREST", static_cast<int>(factors.size()),
                       factors.data(), 0, nullptr, nullptr, nullptr);
  }
  GDALClose(out);

  if (!QFile::exists(outPath) || QFileInfo(outPath).size() <= 0)
    return fail(QStringLiteral("MBTiles 파일이 만들어지지 않았습니다."));
  return true;
}

}  // namespace TilePackService
