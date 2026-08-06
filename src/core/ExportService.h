#pragma once
#include <QString>
class ExportService {
public:
  // Writes README + copies/exports layers. Returns package dir or empty.
  static QString exportSubmissionPackage(const QString& outDir,
                                         const QString& encoding,
                                         const QString& checklistSummary,
                                         bool blockOnError,
                                         bool hasChecklistErrors,
                                         QString* errorOut = nullptr);
  static QString writePdfPlaceholder(const QString& outPath, const QString& title, QString* errorOut = nullptr);
};
