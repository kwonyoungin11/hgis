#pragma once
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <qgsfeatureid.h>
#include <qgsrectangle.h>
class QgsProject;

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

  // 용지 mm × 축척 → 지도 범위(미터 CRS). 입력한 1:N이 그대로 유지되게 한다.
  static QgsRectangle extentForPaperScale(const QgsRectangle& currentExtent,
                                          double mapWidthMm, double scaleDenominator);

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
