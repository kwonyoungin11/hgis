#pragma once
#include <QString>
#include <QStringList>
class QgsProject;
class ExportService {
public:
  static QString exportSubmissionPackage(QgsProject* project,
                                         const QString& outDir,
                                         const QString& encoding,
                                         const QString& checklistSummary,
                                         bool blockOnError,
                                         bool hasChecklistErrors,
                                         QString* errorOut = nullptr);
  static QString writePdfViaLayout(QgsProject* project,
                                   const QString& layoutName,
                                   const QString& outPath,
                                   QString* errorOut = nullptr);
  static bool writeSha256Manifest(const QString& dir, QString* errorOut = nullptr);
};
