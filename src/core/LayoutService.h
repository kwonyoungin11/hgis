#pragma once
#include <QString>
#include <QStringList>
class QgsProject;

class LayoutService {
public:
  enum class Paper { A4, A3 };
  enum class Orientation { Portrait, Landscape };

  static int ensureDefaultLayouts(QgsProject* project);
  static int rebuildDefaultLayouts(QgsProject* project);
  static QString exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                 const QString& pdfPath, QString* errorOut = nullptr);
  static QStringList defaultLayoutNames();
  static QString koreanTitle(const QString& layoutName);

  static QString createReportLayout(QgsProject* project, const QString& titleKo,
                                    Paper paper, Orientation orientation,
                                    QString* errorOut = nullptr);
};
