#pragma once
#include <QString>
#include <QStringList>
class QgsProject;

class LayoutService {
public:
  static int ensureDefaultLayouts(QgsProject* project);
  static int rebuildDefaultLayouts(QgsProject* project);
  static QString exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                 const QString& pdfPath, QString* errorOut = nullptr);
  static QStringList defaultLayoutNames();
  static QString koreanTitle(const QString& layoutName);
};
