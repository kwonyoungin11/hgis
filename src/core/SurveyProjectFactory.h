#pragma once
#include <QString>

class SurveyProjectFactory {
public:
  static QString createNewSurvey(const QString& directory,
                                 const QString& surveyName,
                                 QString* errorOut = nullptr,
                                 const QString& workCrsAuthId = QStringLiteral("EPSG:5186"));
  static const char* defaultWorkCrsAuthId() { return "EPSG:5186"; }
  static const char* uploadCrsAuthId() { return "EPSG:5179"; }
  static const char* defaultCrsAuthId() { return defaultWorkCrsAuthId(); }
};
