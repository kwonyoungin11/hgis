#pragma once

#include <QPointer>
#include <QShowEvent>
#include <QVector>
#include <QWidget>

#include "core/Terrain3dService.h"

class QDoubleSpinBox;
class QLabel;
class QgsMapCanvas;
class QgsProject;
class QgsRasterLayer;
class KaTerrain3dView;

class KaTerrain3dStudio : public QWidget {
  Q_OBJECT
public:
  explicit KaTerrain3dStudio(QgsProject* project, QgsMapCanvas* canvas,
                             QWidget* parent = nullptr);

  bool loadDemPath(const QString& path, QString* errorOut = nullptr);
  bool exportImage(const QString& path, QString* errorOut = nullptr);
  bool exportSheet(const QString& path, QString* errorOut = nullptr);
  bool hasScene() const;
  QImage renderView(int pixelW, int pixelH) const;
  bool groundExtent(double* xMin, double* yMin, double* xMax, double* yMax) const;
  QString workCrsLabel() const;
  double visibleWidthM(int pixelW, int pixelH) const;
  double northYawDeg() const;
  float zMin() const;
  float zMax() const;
  QString demDisplayName() const;
  void setVisibleWidthM(double widthM, int pixelW, int pixelH);
  void refreshDrape();

signals:
  void requestDrawingStudio();

public slots:
  void tryAutoFill();
  void saveImage();
  void runExportSheet();

protected:
  void showEvent(QShowEvent* event) override;

private:
  bool loadDemClipToCanvas(const QString& path, QString* errorOut);
  void finishLoadedScene(const Terrain3dService::DemScene& sc, const QString& path);
  QVector<Terrain3dService::DemCandidate> mapDemCandidates() const;
  void rebuildMesh();
  QImage captureSatellite();
  QgsRasterLayer* ensureGoogleSatLayer();
  QString crsLabel() const;
  void applyTexture(const QImage& tex, const QString& status);

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_canvas;
  QPointer<QgsRasterLayer> m_googleSat;
  KaTerrain3dView* m_view = nullptr;
  QLabel* m_status = nullptr;
  QLabel* m_emptyHint = nullptr;
  QDoubleSpinBox* m_zExag = nullptr;
  Terrain3dService::DemScene m_scene;
  QImage m_texture;
  QString m_demPath;
  QString m_fileDir;
  QString m_textureKind;
  bool m_haveScene = false;
  bool m_autoBusy = false;
  QString m_lastFillKey;
};
