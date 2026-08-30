#pragma once
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <qgsfeatureid.h>
#include <qgsrectangle.h>
class QgsProject;
class QgsLayout;
class QgsLayoutItemMap;

class LayoutService {
public:
  enum class Paper { A4, A3 };
  enum class Orientation { Portrait, Landscape };

  enum class DrawingKind {
    SurveyAreaMap,
    SiteLocation,
    FeaturePlan,
    FeatureDetail,
    Section
  };

  enum class ExtentMode {
    Auto,
    SurveyArea,
    Canvas,
    SelectedFeature,
    WideContext
  };

  struct DrawingRecipe {
    DrawingKind kind = DrawingKind::SurveyAreaMap;
    QString layoutId;
    QString titleKo;
    QString purposeKo;
    double defaultScale = 5000.0;
    double gridIntervalM = 100.0;
    bool includeBasemap = true;
    QStringList layerKeys;
    QString emptyHintKo;
    QList<double> scaleChoices;
  };

  struct DrawingOptions {
    QString titleKo;
    QString surveyName;
    QString siteName;
    Paper paper = Paper::A4;
    Orientation orientation = Orientation::Landscape;
    double scaleOverride = 0.0;
    ExtentMode extentMode = ExtentMode::Auto;
    QgsRectangle canvasExtent;
    QgsFeatureId featureId = FID_NULL;
  };

  struct DrawingBuildResult {
    QString layoutId;
    double appliedScale = 0.0;
    QgsRectangle appliedExtent;
    bool hasMapContent = false;
    QString warningKo;
  };

  static int ensureDefaultLayouts(QgsProject* project);
  static int rebuildDefaultLayouts(QgsProject* project);
  static QString exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                 const QString& pdfPath, QString* errorOut = nullptr);
  static QStringList defaultLayoutNames();
  static QString koreanTitle(const QString& layoutName);

  static QString createReportLayout(QgsProject* project, const QString& titleKo,
                                    Paper paper, Orientation orientation,
                                    QString* errorOut = nullptr);

  static QString createBlankSheet(QgsProject* project, double widthMm, double heightMm,
                                  const QString& name = QStringLiteral("user_sheet"),
                                  QString* errorOut = nullptr);

  // 도면의 래스터(위성·지적·지질 배경)를 조각내지 않고 한 번에 그리게 한다.
  // QGIS 기본값은 래스터를 여러 조각으로 나눠 그리는데, 타일 배경에서는 조각 하나가
  // 빈 채로 돌아오면 그 사각형만 통째로 비어 위성지도가 반만 나온 것처럼 보인다.
  // 조판을 새로 만들 때와 이미 있는 조판을 열 때 모두 걸어 준다.
  static void applySingleRasterPassRendering(QgsLayout* layout);

  // 용지 mm × 축척 → 지도 범위(미터 CRS). 입력한 1:N이 그대로 유지되게 한다.
  static QgsRectangle extentForPaperScale(const QgsRectangle& currentExtent,
                                          double mapWidthMm, double scaleDenominator);

  // 커서 아래 지점을 고정한 채 확대·축소한 지도 범위.
  // fx, fy는 지도 칸 안에서 커서의 상대 위치(0~1, fy는 위에서 아래로).
  // zoomFactor > 1 이면 확대(범위가 좁아짐).
  static QgsRectangle zoomExtentAtAnchor(const QgsRectangle& extent, double fx, double fy,
                                         double zoomFactor);

  // 지도 칸 아래 고정 띠: 왼쪽 축척자, 그 아래 축척 1:N, 오른쪽 CRS, 맨 오른쪽 방위.
  // 위성/지적 픽셀 위에 올리지 않는다. 도면만들기마다 이 기하를 다시 앉힌다.
  struct SheetChromeRects {
    QRectF map;
    QRectF scaleBar;
    QRectF scaleLabel;
    QRectF crs;
    QRectF north;
  };
  static SheetChromeRects standardSheetChrome(const QRectF& page, const QRectF& requestedMap);

  // 1:20 / 1:40 / 1:50 / 1:100 … 분모가 10으로 끝나는 도면 축척.
  static int niceScaleDenominator(double rawScale);
  // 축척자 한 칸(m). 1-2-4-5 계열.
  static double niceScaleBarSegmentMeters(double mapWidthMm, double scaleDenominator,
                                          int segments = 4);
  static double scaleBarWidthMm(double segmentMeters, int segments, double scaleDenominator);

  /// Ink #111827 / paper #FFFFFF / 0.30 mm outline so Double Box reads on satellite and white paper.
  /// Call after setStyle. Does not change segment mode or linked map.
  static void applySheetScaleBarInk(class QgsLayoutItemScaleBar* sb);

  // 전문 측량도면 도곽: 지브라(흑백 교차) 프레임 + 정수 TM 좌표 주기(상하 수평,
  // 좌우 세로쓰기) + 내부 십자 눈금. 기존 격자는 지우고 하나로 다시 만든다.
  // crosses=false면 내부 눈금 없이 도곽·주기만 그린다(위치도처럼 배경이 촘촘할 때).
  static void applySurveyFrameGrid(QgsLayoutItemMap* map, double intervalM, bool crosses,
                                   bool showCoords);
  // 도곽 격자 간격(m): 종이에서 한 칸이 25~70mm가 되는 1-2-5 계열 값.
  static double niceGridIntervalMeters(double scaleDenominator, double mapWidthMm);
  // 좌표계 한글 원점명(중부원점(GRS80) 등). 표제란·도면 라벨 공용.
  static QString koreanCrsName(const QString& authId);

  static DrawingKind kindFromLayoutId(const QString& layoutId);
  static QString layoutId(DrawingKind kind);
  static DrawingRecipe recipe(DrawingKind kind);
  static QList<DrawingRecipe> allRecipes();

  static DrawingBuildResult buildDrawing(QgsProject* project, DrawingKind kind,
                                         const DrawingOptions& options,
                                         QString* errorOut = nullptr);

  static QImage renderPreview(QgsProject* project, const QString& layoutName,
                              const QSize& imageSize, QString* errorOut = nullptr);

  static int exportDrawingPdfs(QgsProject* project, const QString& outDir,
                               QString* errorOut = nullptr);
};
