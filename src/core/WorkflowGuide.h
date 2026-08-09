#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

enum class WorkflowStep : int {
  Survey = 0,
  Background,
  SurveyArea,
  Features,
  ControlPoints,
  Review,
  Submission
};

struct WorkflowStepState {
  WorkflowStep step;
  QString title;
  QString completionHint;
  QString actionId;
  bool complete = false;
};

class WorkflowGuide {
public:
  static QList<WorkflowStepState> evaluate(
      const QJsonObject& projectState,
      bool hasVisibleReferenceLayer,
      int checklistErrorCount,
      bool packageCreated);

  static bool canCreateSubmission(int checklistErrorCount);
};
