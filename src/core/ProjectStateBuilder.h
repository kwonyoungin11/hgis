#pragma once
#include <QJsonObject>
class QgsProject;
class ProjectStateBuilder {
public:
  static QJsonObject fromProject(QgsProject* project);
  static QJsonObject empty();
};
