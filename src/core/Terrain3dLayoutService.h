#pragma once

#include <QString>
#include <qgscoordinatereferencesystem.h>
#include <qgsrectangle.h>

class QgsProject;

// 입체지형 전용 조판. 2D user_sheet와 이름을 섞지 않는다. 제출 5179 아님.
namespace Terrain3dLayoutService {

inline constexpr const char* kSheetName = "terrain3d_sheet";
inline constexpr const char* kIdPicture = "t3d_picture";
inline constexpr const char* kIdLegend = "t3d_legend";
inline constexpr const char* kIdNorth = "t3d_north";
inline constexpr const char* kIdScale = "t3d_scale";
inline constexpr const char* kIdScaleLabel = "t3d_scale_label";
inline constexpr const char* kIdMap = "t3d_map";
inline constexpr const char* kIdCrs = "t3d_crs";

struct SheetSpec {
  QString pngPath;
  QgsRectangle groundExtent;
  QgsCoordinateReferenceSystem crs;
  double visibleWidthM = 0;
  double yawDegFromNorth = 0;
  QString crsLabel;
  QString demName;
  float zMin = 0;
  float zMax = 0;
};

// terrain3d_sheet를 (재)만들고 방위·축척을 붙인다. 범례는 ensureLegend로만.
QString buildSheet(QgsProject* project, const SheetSpec& spec, QString* errorOut = nullptr);
bool ensureLegend(QgsProject* project, const QString& title, int pointSize, bool bold, bool italic,
                  QString* errorOut = nullptr, bool createIfMissing = true);
bool applyNorth(QgsProject* project, int kind, double sizeMm, QString* errorOut = nullptr);
bool applyScale(QgsProject* project, int denominator, QString* errorOut = nullptr);
bool replacePicture(QgsProject* project, const QString& pngPath, QString* errorOut = nullptr);
double pictureWidthMm(QgsProject* project);
bool applyScaleBarStyle(QgsProject* project, const QString& style, QString* errorOut = nullptr);
bool ensureScaleLabel(QgsProject* project, QString* errorOut = nullptr);
bool ensureCrsLabel(QgsProject* project, QString* errorOut = nullptr);

}  // namespace Terrain3dLayoutService
