#pragma once
#include <QString>

class SurveyProjectFactory {
public:
  // Creates a new file pair only; never overwrites a same-named GPKG or QGZ.
  // The GPKG schema is created without adding empty layers to the workspace.
  static QString createNewSurvey(const QString& directory,
                                 const QString& surveyName,
                                 QString* errorOut = nullptr,
                                 const QString& workCrsAuthId = QStringLiteral("EPSG:5187"));
  static const char* defaultWorkCrsAuthId() { return "EPSG:5187"; }
  static const char* uploadCrsAuthId() { return "EPSG:5179"; }
  static const char* defaultCrsAuthId() { return defaultWorkCrsAuthId(); }
};
