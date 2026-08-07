# GUI 시나리오 체크리스트 (수동)

환경: `.\scripts\dev-env.ps1` 후 `.\scripts\run-ka-hgis.ps1`

## 시나리오 A — 최소 제출 플로우
1. [ ] 새 조사 → 이름 → **작업 CRS 5186 또는 5187** → 폴더 → GPKG
2. [ ] (선택) VWorld/위성 배경, 위치검색으로 현장 이동
3. [ ] 단계3 조사구역 그리기 → 우클릭 완료 → 저장
4. [ ] 단계4 유구 면 → 종류/시대 입력 → 저장
5. [ ] 단계5 기준점 2개 (accuracy_m, PDOP, fix_type)
6. [ ] 단계6 검수 → error 0
7. [ ] 단계7 **5179 SHP 변환** + PDF(유구배치도) + 조판 확인
8. [ ] SHP 패키지 UTF-8 → survey_area.shp + MANIFEST.sha256 확인

## 시나리오 B — CRS
1. [ ] 작업 CRS 5186↔5187 전환 후 지도 중첩 유지
2. [ ] 레이어 선택 → 5179 SHP(업로드용) 생성·CRS 확인
3. [ ] (위험) 이름만 지정 → 좌표 미변환 경고 이해

## 시나리오 C — 지오레퍼
1. [ ] GCP 2점 이상
2. [ ] 스캔 평면도 맞추기 → 월드파일 생성·래스터 로드

자동화 대응: `ka_workflow_tests` (헤드리스 A 상당) + reprojectAndMigrateFields
