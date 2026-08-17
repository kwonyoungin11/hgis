#pragma once

#include <QString>
#include <QVector>
#include <QTransform>
#include <QHash>

class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapLayer;
class QgsRectangle;
class QgsCoordinateReferenceSystem;
class QgsGeometry;

namespace GeorefService {

struct Pair {
  double srcX = 0;
  double srcY = 0;
  double mapX = 0;
  double mapY = 0;
};

struct Affine {
  double a = 1, b = 0, c = 0;
  double d = 0, e = 1, f = 0;
  bool valid = false;
  int pairCount = 0;
  double rmsMeters = 0;
};

bool transform(const Affine& a, double sx, double sy, double* mx, double* my);
bool invert(const Affine& a, double mx, double my, double* sx, double* sy);
QTransform toQTransform(const Affine& a);
double rmsMeters(const Affine& a, const QVector<Pair>& pairs);

Affine fromPairs(const QVector<Pair>& pairs);
Affine fitSrcBoxToExtent(double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
                         const QgsRectangle& dest);
Affine fitRasterToExtent(int pixelW, int pixelH, const QgsRectangle& dest);

QString worldFilePathFor(const QString& imagePath);
QString prjPathFor(const QString& imagePath);
bool writeWorldFile(const QString& imagePath, const Affine& a, QString* errorOut = nullptr);
bool writeSidecarPrj(const QString& imagePath, const QgsCoordinateReferenceSystem& crs,
                     QString* errorOut = nullptr);
bool applyWorldFileToRaster(QgsRasterLayer* layer, const Affine& a,
                            const QgsCoordinateReferenceSystem& crs, QString* errorOut = nullptr);

bool transformGeometry(QgsGeometry* geom, const Affine& a);
bool applyAffineToVector(QgsVectorLayer* layer, const Affine& a,
                         const QHash<qint64, QgsGeometry>& originals,
                         const QgsCoordinateReferenceSystem& destCrs, QString* errorOut = nullptr);

QgsVectorLayer* cloneToMemory(QgsVectorLayer* src, const QString& name, QString* errorOut = nullptr);
QString saveVectorCopyGpkg(QgsVectorLayer* layer, const QString& outPath,
                           const QgsCoordinateReferenceSystem& destCrs, QString* errorOut = nullptr);

bool isAlignableLayer(const QgsMapLayer* layer);
bool isDomainSurveyLayer(const QgsMapLayer* layer);
bool isImagePath(const QString& path);
bool isCadPath(const QString& path);
bool looksUnreferencedRaster(const QgsRasterLayer* layer);
void styleAlignedRasterOverlay(QgsRasterLayer* layer);

}  // namespace GeorefService
