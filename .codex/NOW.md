# NOW — Codex Resume

## 2026-09-10 수치지형도 축척 필터

- 광역 80.862초(122레이어)의 1차 원인은 extent-only coverage가 7도엽 전체를 연 것이다. `visibleAtScale`(>1:25000이면 등고·도로·수계·경계만)과 `stopRendering` 가드를 넣었다.
- 안동 재측정(`field-scale.txt` / `next-run/`): 조사 1:1219 1.124초 15레이어(기존 채택), 35초 무입력 유지, 축소 **19.044초/55레이어**, 복귀 1.583초/15레이어. 80초는 줄었으나 19초는 남음. QA에서 무조작 소실은 재현되지 않음(회색 화소 유지). 현장 소실 원인은 미확정.
- Release `ka-hgis.exe` SHA256 `C0FB744CB277C30CCD7EE53FD7D8A676D87127405D41C8DB39C3B6F9651B95D7` (`scaleChanged`도 coverage 타이머에 연결한 뒤 재링크). 직전 CTest 37/37·smoke 영수증의 EXE 해시(`1A36F5…`)는 이 한 줄 연결 이전이다. import 집중 검사 20/20. 커밋·포터블·publish 없음.

## 수치지형도 자동 받기 — 2026-09-09 최종 전송·지도 적재

- 최신 요구는 **여러 도엽을 한꺼번에 체크해 한 신청서로 받기**다. 공식 `downloadMapData`에 확인된 미보관 도엽 배열 전체를 전달한다. 실제 제주 반경10km의 도두·한림·귀일 3도엽/DXF+XML 6파일 수신을 확인했다(40.514초). 파일끼리의 전송만 Qt 저장 완료 뒤 순서대로 시작하며 도엽마다 로그인/신청하지 않는다.
- 최종 공식 INNORIX 페이지의 HTML5 다운로드 컨트롤 생성/주소/파일 ID/메타데이터를 연결했다. 외부 Agent 설치나 GPT·Codex·MCP 개입 없이 앱 코드로 로그인·신청·동의·파일 수신을 진행한다. 공식 웹페이지는 숨기고 단계·실제 파일 수신량·전체 완료 수·실패/취소 사유만 표시한다.
- 실제 받은 한림·귀일은 SHP33개/213,809피처로 자동 준비·지도 적재됐고 1:25000 Qt/QGIS 화소를 확인했다. 회색 #808080/0.2mm, 읽기 전용 참조 역할, 프로젝트/레이어 EPSG:5186 및 원본 SHA256 불변을 검사했다. 위성/지적과의 독립 측량 정합 검증과는 구분한다.
- 귀일 원본 도곽과 공식 색인은 위도0.025°(중심 약2770m)가 다르다. 기존100m 여유를 늘리거나 좌표를 이동하지 않았다. 원본 H0017334의 닫힘·모든 변·모서리·기하 유효성과 후보별 왕복 잔차를 검증해 유일한 EPSG:5186(최대4.757mm)을 확인했다. 차이는 사용자에게 안내한다. 단일/유일 후보가 아니거나 손상·미폐합·365m 이동·도곽 밖 지형은 자동 적재하지 않는다.
- 도두 원본은 주요 지형 없이 도곽만 있다. 수신 실패나 정상 지도 적재로 혼동하지 않고 검토 대상으로 보관한다. 검토 영수증은 공식 메타데이터·상대경로·원본 SHA256·solverVersion을 확인한다. 이전 판별기 결과는 받은 원본으로 재평가하고 변환/저장 실패에도 재다운로드하지 않는다. 원본과 기존 기록은 보존한다.
- F0027132 표고점수치의 `Text`를 읽어 표시한다. 글자 삽입점 Z=0 때문에 모두0.0으로 표시되던 원인을 수정했다. 실제0m와 다른 표고점의 Z를 보존한다. 확대 중 레이어 제거로 렌더가 취소되면 갱신을 다시 예약한다. 백그라운드 도곽 읽기는 GUI 프로젝트에 접근하지 않는 읽기 전용 QGIS OGR provider를 사용한다.
- 최신 범위는 **10km 고정**. 저장은 현재 조사 폴더/지형도 아래 원본·SHP·index·receipts. 더보기의 VWorld API 키 아래 계정 설정에서 ID/비밀번호를 바꾸며, 변경 시 이전 세션을 폐기한다. 개인정보/비밀번호를 코드나 보고서에 하드코딩하지 않는다.
- 검증: 실제 공식 전송, 실제 파일 변환·지도 화소, 도곽/표고/저장실패/취소 회귀를 확인했다. **Release 구성·빌드, 전체 CTest 37/37(216.35초), 시작 검사 통과**. build/release-verified.json 발급. EXE SHA256 E1EBC6FC4F9371FBC5A0604612D44C8C9E8BE9D5CED875BF5FAE97F17546CEB2. 상세 근거와 캡처는 `build/qa/topographic-complete/REPORT.md`.
- 실행은 바탕 화면 **고고학 전용 HGIS** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`. 사용자 실행 창을 종료/재시작/조작하지 않는다. 포터블/publish/dist/L: 복사는 새로 명시적 요청받을 때만. 이번 작업 커밋·푸시·포터블 없음.

## DEM 연속 표현 — 2026-09-09

- 최신 사용자 지시에 따라 DEM은 전국 고정 0–2000m 연속 보간, 저지대에 조밀한 16개 표고 기준점, 공통 세로 색띠(표고 m)로 변경했다. 전국/저지대/화면맞춤 프리셋을 제공하며 화면맞춤은 이동 후 눈금도 갱신한다.
- DEM 음영은 기본 ON으로 최신 요구를 적용했다. 미터 좌표계 VRT, 다방향·Multiply·기본 z=1/강도30%, 표고 샘플의 provider bilinear 보간을 사용한다. 사용자 OFF 및 저장/재열기 설정을 보존한다. 이전 'DEM 음영 자동 추가 제거' 보류 기록보다 이번 명시적 요구가 우선한다.
- 당시 수치지형도 공급원 0단계 조사를 마쳤다. 이후 사용자의 자동 다운로드 구현 요청으로 진행 상태가 바뀌었으며 위 최신 항목을 따른다. 지형맵(OpenTopoMap PNG XYZ)과 수치지형도 벡터는 별개다.
- 이후 사용자 지시로 포터블/publish는 새로 명시적으로 요청한 경우에만 실행한다. 검증 대상은 바탕 화면 아이콘이 여는 현재 Release다.
- 검증 상세와 전후 화소: build/qa/dem-continuous/REPORT.md 및 gallery.html. 완료 여부는 해당 보고서의 최종 실행 결과로 판단한다.


## Current State (2026-09-08)

- 사용자 최신 지시: 새로 명시적으로 요청하기 전에는 포터블 제작·publish·dist/L: 복사를 실행하지 않는다. 이전 포터블 요청과 AGENTS의 일반 publish 절차보다 우선한다. 사용자 실행 앱도 종료·재시작·자동 조작하지 않는다.
- 사용자 고정 실행 경로는 바탕 화면 **고고학 전용 HGIS.lnk** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`다. 실제 바로가기 대상을 확인했고 그대로 유지했다. Release가 없을 때 다른 빌드/포터블을 실행하던 fallback을 제거했다. AGENTS Verification도 이 경로와 요청 시만 포터블 제작으로 갱신했다.
- 최신 수정 범위는 주변 거리 경계의 중복 표시, 시도 선택 이동, 요청하지 않은 「현장 지도」 버튼 제거, 좁고 긴 레이어의 전체 범위 이동이다. 자동 검증 결과와 화소는 `build/qa/navigation-repair/REPORT.md`에 기록한다. 기존 전체 Phase가 모두 완료된 것은 아니다.
- 위 이동/중복 수정은 Release·전체 CTest 24/24(192.44초)·headless 시작 검사 통과로 마무리했다. 후속 요청은 **면적 기본 ON 및 글자 크기 변경 시 표시 보존 → 시굴격자 면적10%/길이≤20m/폭≤2m/구역 내부 → 도면 정보 아래 공백을 활용한 행간과 작은 화면 스크롤** 순으로 처리 중이다. 각 묶음 검증 후 다음으로 진행한다. 면적 수정 보고는 `build/qa/label-area-repair/`에 기록한다.
- 면적 표시 수정 완료: 새 조사 폴리곤 면적 기본 ON, 글자 크기 변경 및 후속 편집에서 표시식·체크·숨김 보존. 독립 리뷰의 크기 식 재정의 문제도 보완했다. Release·전체 CTest 24/24(190.34초)·headless 시작 검사와 실제 Qt/QGIS 전후 화소가 통과했다. 보고: `build/qa/label-area-repair/REPORT.md`. 다음은 시굴격자 규격이며, 도면 정보 패널은 그 다음 묶음이다.
- 시굴격자 수정 완료: 자동 생성 10%/2%, 폭≤2m·길이≤20m·구역 내부·비중복, 재생성 트랜잭션과 미저장 편집 보존. 25×115m 검사에서 길이 24m/면적 288㎡ → 최대 길이 19.6918m/면적 287.50㎡. 기하 49/49·통합 2조건·전체 CTest 24/24(196.72초)·headless 시작 검사 통과. 소스별 실제 QGIS 전후 화소: `build/qa/trench-limits/REPORT.md`.
- 도면 정보 패널 수정 완료: 아래 공백을 행간으로 분배하고 최소 8px 간격, 카드 내용 높이 보존, 작은 창 세로 스크롤과 최소 폭을 적용했다. 1800×1150 검사에서 마지막 행 아래 공백 358→17px. 일반/큰/작은 창 및 150% 배율 집중 검사 각 6/6, 전체 Release·CTest 24/24(196.79초)·headless 시작 검사 통과. 자동 Qt 전후 화소와 로그: `build/qa/drawing-panel-spacing/REPORT.md`. 면적 표시·시굴격자·패널의 후속 세 묶음을 각각 검증하고 마무리했다. 사용자 앱을 조작하거나 포터블을 제작하지 않았다.


### 회귀 안정화 — 2026-09-08 (이전 자동 검증 20/20, 사용자 실측 확인은 별도)

- 사용자가 “하나를 고치면 다른 기능이 고장 난다”는 회귀를 우선 해결하도록 요구했다. 하천명 표시·DEM의 지형 음영 자동 추가 제거·새 지질도 공급원 연결은 보류하고 이번 안정화 결과를 보고한 뒤 멈춘다.
- 실제 16:13 지형맵/16:01 지적 추가 종료 스택은 위성 정렬의 `QgsLayerTreeGroup::reorderGroupLayers`에 모인다. 설치 SDK의 삭제 노드 재참조 경로를 피하도록 앱의 3호출을 `LayerOps::moveLegendLayer`로 대체했고, 중복 정리 전부터 재진입을 막는다. 복제 삽입 후 기존 노드만 제거하여 원본 데이터와 레이어 수명을 보존한다.
- 주제도 100001 축척 제한을 해제했다. 기존 앱 생성 자료의 같은 제한만 열기 후 메모리에서 갱신하고, 외부 자료의 사용자 제한은 보존한다. 새 다운로드 범위/지질 80 km·수계 160 km 제한은 유지한다.
- 주소 팝업을 클릭한 모니터 작업영역 안에 배치하고 작은 화면에서는 줄바꿈한다. 일반·150% DPI 각각 QtTest 9/9 및 자동 화소 확인을 마쳤다.
- 별도 WMS 종료 스택의 부분 렌더 중첩을 완화하도록 `RenderPartialOutput`을 해제했다. 실제 로컬 XYZ의 타일 수신 중 이동·취소·최종 화소 검사(5186/5187)는 통과했지만 이전 부분 출력 설정도 같은 합성 검사에서 통과했다. 따라서 이 별도 현장 종료를 완전히 재현·해결했다고 표시하지 않는다.
- 앞선 공통 우클릭 복구, Delete 레이어 제거와 Ctrl+Z 복원, 꼭짓점·일괄 삭제 Undo, DEM/오프라인 다운로드 진행·취소, 큰 축척 분모에서 팬 수정도 현재 코드에 보존되어 전체 검사 대상이다. 병합·격자 재생성 등 모든 Undo 경로와 과거 전체 Phase가 완료된 것은 아니다.
- `scripts/verify-release.ps1`이 구성·Release 빌드·전체 CTest·headless 시작 검사 후 소스/테마/테스트/EXE 해시 기록을 남긴다. `publish-desktop.ps1`은 이 기록과 현재 입력이 일치해야 복사하며, 실행 중인 사용자 앱은 강제 종료하지 않고 배포를 거부한다. Codex 전역 hooks 설정은 변경하지 않았다.
- 최종 전체 CTest **20/20, 201.25초**, Release 시작 검사, D: portable의 시스템 PATH만 남긴 직접 EXE 시작 검사 exit 0. EXE SHA256 **4C09CC66E760304576CB5704E3680DDCF38BEA3A3F4D2F86FDA158E8D61FBA8B**. 원본 조사 파일 수정 및 커밋 없음.
- 실제 사용자 앱의 화면 자동 조작은 중단 요청을 유지하여 실행하지 않았다. 자동 Qt/QGIS 합성 캡처를 실제 현장 portable 화면 검증으로 대체하지 않는다. 보고/화소/로그: `build/qa/regression-repair/REPORT.md`. L: 이동식 포터블 배포 결과도 이 보고서에 기록한다.

### 앱 전체 테마·광택·축척 추가 — 2026-09-08 (자동 검증·배포 완료, 실제 화면 확인 미완료)

- 사용자의 후속 요청에 따라 버튼 비례에 이어 **앱 전체 테마·아이콘을 실제로 변경**했고, 가장 최근 요청인 **10000·25000 축척 추가, UI 색 20% 완화, 광택 강화**까지 구현했다. 앞선 단계 순서만을 이유로 명시적인 후속 요청을 보류하지 않는다.
- 크림 바탕을 쿨그레이/차콜/청록빛 파랑의 공통 팔레트로 통일했다. 수계는 파랑, 토양은 갈색·황토층과 검은 윤곽이며 기능별 아이콘 색과 상태가 구분된다. 리본·홈·레이어/파일함·메뉴·입력·상태표시줄·지도/조판/사진 정합의 UI 바탕에 같은 테마를 적용했다. 지도 심벌과 PDF 용지 내용 색은 보존한다.
- 최신 완화 정책: 기능 채움색의 HSL 채도는 직전의 80%, 밝은 표면은 기존 RGB 80%+흰색 20%. 글자·윤곽·어두운 홈 레일은 가독성을 위해 보존한다. 상단 반사광 띠와 중간톤/하단 음영을 강화했다. 팔레트 계산 최저 대비 4.821:1, 실제 자동 위젯의 검사한 상태 중 최저 5.251:1.
- 도면 정보는 기존 3열 정렬에 10000/25000을 한 행 추가했다. `syncScaleChips()`가 실제 QToolButton을 찾게 고쳐 선택 표시가 하나만 남는다. 합성 EPSG:5187 스튜디오에서 버튼 클릭→지도 축척·입력값·선택 상태를 검사한다.
- 전체 Release 빌드, 테마 21개 검사, build/portable 시작 스모크와 배포가 통과했다. 첫 전체 CTest의 workflow QSS 검사는 고정 220자 제한 때문에 실패해 해당 규칙의 닫는 중괄호까지 확인하도록 고쳤다. 캡처 fixture의 한글 글꼴·실제 테마 적용도 보완했다. 수정 후 최종 전체 CTest **17/17(179.27초)**가 통과했다. 테마 21개와 실제 축척 버튼 단독 검사도 통과했다.
- 배포 실행 파일 SHA256은 `D1B11AD5FE89580F62B4EF76925996937EB415D0DEA62333FE5A34FC8DA9592B`이며 Release와 portable가 동일하다. 소스/portable QSS도 동일하다. `publish-desktop.ps1` 실행 직전 HGIS 프로세스가 없음을 확인했고 사용자 앱을 강제 종료하지 않았다.
- **실제 portable 변경 후 화면·PDF 검증은 미완료**다. native 창 목록 조회에서 사용자 물리 Escape 중단이 반환되어 이번 턴에 추가 Computer Use를 하지 않는다. 자동 Qt 렌더와 기존 사용자 캡처를 실제 portable 변경 후 검증으로 대신하지 않는다. 다음 단계 자동 진행과 전체 요구 완료 판정은 하지 않는다.
- 최신 상세 보고·팔레트·전후 자동 화소·검증 로그: `build/qa/pro-ui-soft20/REPORT.md`. 이전 전체 테마/아이콘 근거는 `build/qa/pro-ui-step2/`, 버튼 비례 기준은 `build/qa/pro-ui-step1/`. 사용자 원본 조사 파일을 수정하지 않았고 커밋하지 않았다.

### 고고학 전용화 1단계 — 검증 미완료 (2026-09-08)

- 이전 사용자 요청은 1 우클릭 메뉴 → 2 UI/동선/Undo → 3 격자 → 4 조판 → 5 전체 GIS 정리였다. 1단계 구현을 보존했고 2~5단계는 완료하지 않았다. 현재 우선순위는 위의 최신 전문가용 UI 요청이다.
- 새 `MainWindowContextMenus.cpp`에서 layer_key/명시적 역할/공급자로 메뉴를 나눈다. 불필요 항목은 제외, 일시적으로 불가능한 명령은 비활성+한국어 툴팁, 삭제는 마지막 그룹. 우클릭 행과 작업 대상을 일치시키고 목록 제거와 원본 도형 삭제를 분리했다.
- 선택 도구 메뉴도 조사 layer_key에만 편집 항목을 제공한다. 이전/다음 지도 범위는 QGIS 기존 기능을 사용한다. 가져온 래스터/CAD에는 imported_reference 표식을 보존하며 정합 결과 객체에도 승계한다.
- 작업 전 소스 사본/합성 9종 fixture/변경 전 캡처 8개/단계 전용 패치: `build/qa/archaeology-step1/`. 상세 상태는 같은 폴더 `REPORT.md`.
- **사용자가 물리 Esc로 Computer Use를 중단했다. 이 턴에서는 추가 UI 조작을 하지 않는다.** 지적 WMS 메뉴는 관찰했지만 파일 저장 전에 중단됐고 지도 캔버스 및 변경 후 9종 portable 캡처는 남았다.
- 앞서 사용자 조사 앱이 Release 실행 파일을 잠가 LNK1104가 발생했으나 사용자가 앱을 닫아 해소됐다. 우클릭 후 창 종료 시 지도 도구가 캔버스보다 늦게 파괴되는 문제를 소멸자 순서로 수정했고, 전체 CTest에서 save_open_window와 workflow가 통과했다. 최종 빌드·검사 로그는 전문가용 UI 단계에 함께 기록한다. 메뉴 9종 변경 후 화면 검증은 여전히 미완료다.
- Ctrl+Z/삭제 키 전체, UI 글자·크기·색·여백, 격자 알고리즘과 조판 흐름은 이번 단계 변경에 포함하지 않았다. 저장되지 않은 도형 편집의 Undo/레이어 제거 키 연결은 2단계에서 우선 확인해야 한다. 기존 원본 데이터와 다른 미커밋 변경을 보존하고 커밋하지 않았다.

### 현장 완성도 — Phase 0 / Phase 1 (2026-09-08)

- 사용자는 Phase 0 측정 후 Phase 1 안정화까지 진행하고, 각 Phase 끝에서 멈춰 보고하도록 요청했다. Phase 2~6을 이어서 자동 진행하지 않는다.
- Phase 0은 제품 수정 없이 약 13분 동안 측정했다. 실제 portable 캡처와 24개 참조 레이어/24,000 피처 반복 측정, 작업 전 소스 사본은 `build/qa/production-phase0/REPORT.md`와 같은 폴더에 있다.
- Phase 1: 새 조사 동명 파일 보호, QGZ 원자적 교체·실패 복원, Save As 실패 시 원본 소스/메모리 피처 복원, 닫기 저장의 메모리 벡터 보관, 조사 전환 전 미저장 확인, 저장 중 닫기 재진입 차단을 구현했다. 다른 이름으로 저장은 기존 대상 GPKG/QGZ를 덮어쓰지 않고 새 이름을 요구한다.
- 위치/행정경계 요청은 전체 20초, 참조지도 요청은 각 15초 제한과 취소를 적용했다. 토양·지질·수계 및 고지형의 토양 준비는 QgsTask에서 수행하고 GUI에는 값과 파일만 전달한다. 종료·조사 전환 뒤 늦은 결과는 적용하지 않는다.
- 설치 QGIS 4.3 `QgsArchive::zip`의 공통 `qgis-project-XXXXXX.zip` 경합을 확인했다. 앱은 Qt/QGIS 생성 전 프로세스별 TEMP/TMP를 분리하며, 생성된 래스터의 저장 후 참조를 보존하려고 자동 삭제하지 않는다. CTest도 각 테스트별 임시 폴더를 사용한다.
- 꼭짓점 이동의 저장 실패는 편집 버퍼를 유지하고 한국어로 알린다. 화면 CRS와 레이어 CRS가 다를 때 좌표 변환도 보완했다.
- 최종 Release 빌드, CTest **17/17 (152.32초)**, 시작 스모크, portable 배포가 통과했다. Release/portable SHA256은 `B4FC088F23430D42B248C04060FCC80EFF6321FDB8326F205E6587D0ECF24FBC`로 일치한다. 실제 portable 화면 확인과 재측정 결과는 Phase 1 보고서에 마무리한다.
- Phase 1 산출물: `build/qa/production-phase1/`. 기존 미커밋 변경과 원본 조사 자료를 보존했다. 커밋하지 않았다.

### New survey work CRS verification (2026-09-08)

- Fixed the stale work-CRS status chip on new survey creation and saved-project reopen. The chip now follows the canvas destination CRS change signal; a selected CRS is committed to the session only after survey creation succeeds.
- Verified both EPSG:5186 and EPSG:5187 through the real portable application's new-survey dialog, save, and explicit reopen. Screenshots: `build/qa/crs-selection/{created,reopened}-{5186,5187}.jpg`.
- Regression tests assert project CRS, canvas CRS, GPKG layer CRS, manual status-chip switching, reopen, and the invariant submission chip 5179.
- Release build and isolated smoke passed (exit 0). Final CTest: 14/14 passed, exit 0, 167.16 seconds. Corrected two existing test fixtures (hidden canvas output-size initialization; actual polygon required for composed layout) without relaxing their checks or changing product GIS/layout behavior.
- Published portable executable matches the Release SHA256. Original field data and the geology 80 km / river 160 km ranges were not changed. No commit.

The current requested startup behavior is **home screen only**. On launch, ka-hgis opens the application shell and waits for the user to choose **조사 열기**, a recent survey item, or **새 조사**.

Automatic startup restore is intentionally disabled. Do not reintroduce automatic LayersOnly restore, last-project restore, drawing restore, basemap restore, WMS/XYZ restore, or background project loading on app startup.

The previous automatic restore path loaded 31 layers, including a 390k-lot cadastral layer, and missed the aerial imagery. The measured startup/log delay included survey opening work that should happen only after the user explicitly opens a survey.

Current implementation direction:

- Constructor-time map/background initialization and scheduled last-survey restore were removed or disabled by default.
- Explicit open paths remain valid: **조사 열기**, recent survey click, and **새 조사**.
- Do not modify original survey data during startup.
- Do not commit unless the user explicitly asks.

Validation expected for future C++ startup/UI work:

- configure/build Release with the repo PowerShell commands;
- run `ctest --test-dir build -C Release --output-on-failure`;
- run `.\scripts\run-ka-hgis.ps1 --smoke-quit`;
- run `.\scripts\publish-desktop.ps1` for field-facing UI changes.

Docs/settings-only work does not require rebuilding the C++ application.

## Recent Product Notes To Preserve

- 안동 뷰어 work is separate from the main ka-hgis product. Do not fold it into `MainWindow` unless explicitly requested.
- VWorld keys remain local only. Do not hardcode a production key.
- `removeAllMapLayers()` remains forbidden in survey-load flows.
- Work CRS may be EPSG:5186 or EPSG:5187; upload/export output stays EPSG:5179 SHP + PDF + MANIFEST.
- Domain layers appear in the legend only after draw/import/open of actual user data.
- Reference maps stay separate from survey data.

## Harness Migration

This project now treats `AGENTS.md`, `.codex/NOW.md`, `HANDOFF.md`, and `docs/HANDOFF.md` as the active Codex-native guidance surface.

Legacy `.grok`, `.cursor`, `.agents` dispatch/history files, and Orca files may remain for history or compatibility, but they are not the active preset source for new Codex work. The project-local `.agents/skills/ka-hgis-gis/SKILL.md` is the maintained Codex GIS specialization. Do not add new project-level Grok, Antigravity, Cursor-only, or old fixed model routing requirements unless the user explicitly asks for that compatibility layer.

## Project-local GIS setup (2026-09-08)

General C++/GPT-6 settings remain global. HGIS GIS instructions and the `ka-hgis-gis` skill live only in this repository. The skill links GIS evidence, source areas and relevant regression tests. Historical C++17/LTR and 5179 schema notes are distinguished from the current toolchain and survey CRS selection. This is a documentation/skill configuration change, not a product behavior change or new application test run.
