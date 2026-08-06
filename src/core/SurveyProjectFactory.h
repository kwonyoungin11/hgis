#pragma once
#include <QString>

class SurveyProjectFactory {
public:
  static QString createNewSurvey(const QString& directory,
                                 const QString& surveyName,
                                 QString* errorOut = nullptr);
  static const char* defaultCrsAuthId() { return "EPSG:5179"; }
};
