#include "WorkflowGuide.h"

QList<WorkflowStepState> WorkflowGuide::evaluate(
    const QJsonObject& projectState,
    bool hasVisibleReferenceLayer,
    int checklistErrorCount,
    bool packageCreated) {
  QList<WorkflowStepState> steps;

  // Step 0: Survey (새 조사)
  bool surveyDone = !projectState.value(QStringLiteral("survey_name")).toString().isEmpty() ||
                    projectState.value(QStringLiteral("layer_count")).toInt(0) > 0 ||
                    projectState.value(QStringLiteral("survey_area_count")).toInt(0) > 0;
  steps.append({
      WorkflowStep::Survey,
      QStringLiteral("1. 새 조사"),
      QStringLiteral("조사 프로젝트를 생성하거나 기존 프로젝트를 열어주세요."),
      QStringLiteral("action_new_survey"),
      surveyDone
  });

  // Step 1: Background (배경·지적도)
  steps.append({
      WorkflowStep::Background,
      QStringLiteral("2. 배경·지적도"),
      QStringLiteral("VWorld 배경지도나 수치지형도를 추가하세요."),
      QStringLiteral("action_add_basemap"),
      hasVisibleReferenceLayer
  });

  // Step 2: SurveyArea (조사구역)
  bool surveyAreaDone = projectState.value(QStringLiteral("survey_area_count")).toInt(0) > 0;
  steps.append({
      WorkflowStep::SurveyArea,
      QStringLiteral("3. 조사구역"),
      QStringLiteral("조사구역 경계 폴리곤을 디지타이징하세요."),
      QStringLiteral("action_digitize_area"),
      surveyAreaDone
  });

  // Step 3: Features (유구·단면선)
  bool featuresDone = projectState.value(QStringLiteral("feature_poly_count")).toInt(0) > 0 ||
                      projectState.value(QStringLiteral("feature_line_count")).toInt(0) > 0;
  steps.append({
      WorkflowStep::Features,
      QStringLiteral("4. 유구·단면선"),
      QStringLiteral("발굴 유구 폴리곤 및 단면선을 그려 넣으세요."),
      QStringLiteral("action_digitize_feature"),
      featuresDone
  });

  const int cpCount = projectState.value(QStringLiteral("control_points_count")).toInt(0);
  const bool cpMeta = projectState.value(QStringLiteral("has_datum")).toBool() &&
                      projectState.value(QStringLiteral("has_ellipsoid")).toBool() &&
                      projectState.value(QStringLiteral("has_projection")).toBool();
  const bool cpDone = cpCount >= 2 && cpMeta;
  steps.append({
      WorkflowStep::ControlPoints,
      QStringLiteral("5. GPS 기준점"),
      QStringLiteral("기준점 2개 이상 + 측지기준/타원체/투영 메타를 입력하세요 (CSV 가능)."),
      QStringLiteral("action_add_control_point"),
      cpDone
  });

  // Step 5: Review (도면 검수)
  bool reviewDone = (checklistErrorCount == 0);
  steps.append({
      WorkflowStep::Review,
      QStringLiteral("6. 도면 검수"),
      QStringLiteral("체크리스트 검수 오류 0건을 달성하세요."),
      QStringLiteral("action_run_checklist"),
      reviewDone
  });

  // Step 6: Submission (제출 패키지)
  steps.append({
      WorkflowStep::Submission,
      QStringLiteral("7. 제출 패키지"),
      QStringLiteral("최종 제출용 SHP/PDF 패키지(+MANIFEST)를 출력하세요."),
      QStringLiteral("action_export_package"),
      packageCreated
  });

  return steps;
}

bool WorkflowGuide::canCreateSubmission(int checklistErrorCount) {
  return checklistErrorCount == 0;
}
