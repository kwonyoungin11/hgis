# GUI 시나리오 체크리스트 (수동)

환경: `.\scripts\dev-env.ps1` 후 `.\scripts\run-ka-hgis.ps1`

## 시나리오 A — 최소 제출 플로우
1. [ ] 새 조사 만들기 → 폴더/이름 지정 → GPKG 생성
2. [ ] (선택) 도구 → OSM 배경 추가
3. [ ] 단계3 조사구역 그리기 → 우클릭 완료 → 저장
4. [ ] 단계4 유구 면 → 종류/시대 입력 → 저장
5. [ ] 단계5 기준점 2개 (accuracy_m, PDOP, fix_type)
6. [ ] 단계6 검수 → error 0
7. [ ] 단계7 PDF (유구배치도) 저장, 파일 크기 > 1KB
8. [ ] SHP 패키지 UTF-8 → survey_area.shp + MANIFEST.sha256 확인

## 시나리오 B — CRS
1. [ ] 레이어 선택 → 좌표계 → 이름만 지정(위험) → 라벨만 바뀜
2. [ ] 좌표계 → 좌표 변환 → EPSG:4326 gpkg 생성·추가

## 시나리오 C — 지오레퍼
1. [ ] GCP 2점 이상
2. [ ] 스캔 평면도 맞추기 → 월드파일 생성·래스터 로드

자동화 대응: `ka_workflow_tests` (헤드리스 A 상당) + reprojectAndMigrateFields
