#pragma once
#include <QString>
class QgsProject;
class LayoutService {
public:
  // Ensures 5 named print layouts exist on project (creates if missing).
  static int ensureDefaultLayouts(QgsProject* project);
  // Export named layout to PDF. Returns path or empty.
  static QString exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                 const QString& pdfPath, QString* errorOut = nullptr);
  static QStringList defaultLayoutNames();
};
