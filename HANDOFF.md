# ka-hgis Handoff — product SSOT (v0.3.0)

## 2026-09-10 포터블은 다른 PC 이전 설치를 무시

포터블은 VWorld 키·최근 조사·창 배치·지적 XML·타일 캐시·QGIS 프로필을 **exe/config·exe/cache**만 쓴다. 소스에 API 키를 넣지 않는다. 키는 폴더 `config/secrets.ini`(이 PC에서 복사). 다른 PC AppData/레지스트리의 빈 키·옛 최근목록은 무시. 위성·지적은 홈이 아니라 **새 조사/조사 열기** 뒤 범례에 올라간다. 커밋 없음.

## 2026-09-10 다른 PC 포터블 좌표계

포터블은 개발 PC의 OSGeo 설치 경로가 아니라 **자기 폴더의 proj.db**로 5186/5187/3857을 읽는다(`KaPortableRuntime`, Wide+UTF-8 env, `OSRSetPROJSearchPaths`). EXE만 옮기면 위성·지적이 안 뜬다. 폴더 전체를 복사하고 `ka-hgis.exe` 또는 `start.bat`으로 실행. 모니터 배율은 기존 High DPI PassThrough. 커밋 없음.

## 2026-09-10 조판 전체끄기 · 수치지형 합치기 · 지질 범례

조판 레이어 카드에도 맵과 같은 전체끄기/전체켜기가 있다(조판은 `syncMapFromLayers`만). 수치지형도는 같은 종류+기하를 한 범례 줄로 합친다(`등고선 · 선`). 표고점은 받지 않는다. 조판 지질 범례는 맵에 있는 지층·단층만 두고 부정합·주향경사·지질경계·단층추정은 뺀다. 옛 창을 닫고 바탕 **고고학 전용 HGIS**.

## 2026-09-10 수치지형도 유적위치도 분류

줌할 때마다 식생·시설·지명을 넣었다 빼서 렉이 났다. 지금은 등고·도로·수계·경계·건물만 올리고 줌으로 분류를 바꾸지 않는다. 집은 NGII 건물면이 있으면 연한 회색 면으로 보인다. 선/면이 다른 SHP는 범례가 `등고선 · 선` / `등고선 · 점`처럼 갈라진다(QGIS 한 레이어 한 기하). 옛 창을 닫고 바탕 **고고학 전용 HGIS**.

## 2026-09-10 수치지형도 10km 커버리지 적재

받기 문구의 「도엽 N장」은 보관·준비 수이고, 범례는 조사창(수백 m)과 겹치는 SHP만 올리고 있었다. 적재 질의를 `coverageBounds`(화면 ∪ 중심 ±10km 정사각)로 바꿔 받은 이웃 도엽이 조사 줌에서도 올라간다. 20km 밖·100km 팬은 기존처럼 언로드. 제주 도두(도곽만)는 검토 보관·자동 적재 없음. 옛 창을 닫고 바탕 **고고학 전용 HGIS**. 상세: `docs/HANDOFF_TOPOGRAPHIC_GROK.md`.

## 2026-09-10 Win11 Bloom 크롬 테마

첨부 Bloom 벽지 풍으로 크롬 전체에 mica 워시를 넣었다. 창·홈·리본·상태바는 `selectedTop`/`surface`/`hoverBottom` 그라데이션, 168px 레일만 선명한 `#0067C0`+흰 글자. 입력칸·툴팁·맵 카드·투명도 레일은 단색. 지도/조판 캔버스 exclude 유지. 홈 `startMain`은 테두리 없이 워시만(이중 프레임 제거). railMuted `#F3FBFF`. 아이콘 selected `#163F59`. 커밋·포터블 없음. 옛 창을 닫고 바탕 **고고학 전용 HGIS**.

## 2026-09-10 수치지형도 축척 필터

넓은 범위 이동이 7도엽 122개를 모두 열어 80.862초가 걸리던 원인을 화면 축척 필터로 줄였다. 1:25000보다 작으면 등고·도로·수계·경계만 올린다. 안동 재측정은 축소 19.044초/55레이어. 무조작 지도 소실은 QA에서 재현되지 않았고 현장 원인은 미확정이다. 상세: `docs/HANDOFF_TOPOGRAPHIC_GROK.md`.

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


> Updated: 2026-09-08. Codex-native C++/Qt/QGIS session. 현재 세션: [`.codex/NOW.md`](./.codex/NOW.md)

**Agent rules:** [`AGENTS.md`](./AGENTS.md)  
**This file + `docs/HANDOFF.md`:** edit together.

**HGIS 전용 Codex 스킬:** [ka-hgis-gis](.agents/skills/ka-hgis-gis/SKILL.md). 전역 GPT-6/C++ 설정을 상속하고 이 저장소에서만 GIS 규칙을 추가한다. 과거 `.agents` 작업 기록과 구분한다.

---

## 주변 거리 표시·지역/레이어 이동 — 2026-09-08

- 사용자 최신 지시: 새로 명시적으로 요청하기 전에는 포터블 제작·publish·dist/L: 복사를 실행하지 않는다. 이전 포터블 요청과 AGENTS의 일반 publish 절차보다 우선한다. 사용자 실행 앱도 종료·재시작·자동 조작하지 않는다.
- 사용자 고정 실행 경로는 바탕 화면 **고고학 전용 HGIS.lnk** → `scripts/start-ka-hgis.vbs` → `launch.ps1` → `build/Release/ka-hgis.exe`다. 실제 바로가기 대상을 확인했고 그대로 유지했다. Release가 없을 때 다른 빌드/포터블을 실행하던 fallback을 제거했다. AGENTS Verification도 이 경로와 요청 시만 포터블 제작으로 갱신했다.
- 최신 수정 범위는 주변 거리 경계의 중복 표시, 시도 선택 이동, 요청하지 않은 「현장 지도」 버튼 제거, 좁고 긴 레이어의 전체 범위 이동이다. 자동 검증 결과와 화소는 `build/qa/navigation-repair/REPORT.md`에 기록한다. 기존 전체 Phase가 모두 완료된 것은 아니다.
- 위 이동/중복 수정은 Release·전체 CTest 24/24(192.44초)·headless 시작 검사 통과로 마무리했다. 후속 요청은 **면적 기본 ON 및 글자 크기 변경 시 표시 보존 → 시굴격자 면적10%/길이≤20m/폭≤2m/구역 내부 → 도면 정보 아래 공백을 활용한 행간과 작은 화면 스크롤** 순으로 처리 중이다. 각 묶음 검증 후 다음으로 진행한다. 면적 수정 보고는 `build/qa/label-area-repair/`에 기록한다.
- 면적 표시 수정 완료: 새 조사 폴리곤 면적 기본 ON, 글자 크기 변경 및 후속 편집에서 표시식·체크·숨김 보존. 독립 리뷰의 크기 식 재정의 문제도 보완했다. Release·전체 CTest 24/24(190.34초)·headless 시작 검사와 실제 Qt/QGIS 전후 화소가 통과했다. 보고: `build/qa/label-area-repair/REPORT.md`. 다음은 시굴격자 규격이며, 도면 정보 패널은 그 다음 묶음이다.
- 시굴격자 수정 완료: 자동 생성 10%/2%, 폭≤2m·길이≤20m·구역 내부·비중복, 재생성 트랜잭션과 미저장 편집 보존. 25×115m 검사에서 길이 24m/면적 288㎡ → 최대 길이 19.6918m/면적 287.50㎡. 기하 49/49·통합 2조건·전체 CTest 24/24(196.72초)·headless 시작 검사 통과. 소스별 실제 QGIS 전후 화소: `build/qa/trench-limits/REPORT.md`.
- 도면 정보 패널 수정 완료: 아래 공백을 행간으로 분배하고 최소 8px 간격, 카드 내용 높이 보존, 작은 창 세로 스크롤과 최소 폭을 적용했다. 1800×1150 검사에서 마지막 행 아래 공백 358→17px. 일반/큰/작은 창 및 150% 배율 집중 검사 각 6/6, 전체 Release·CTest 24/24(196.79초)·headless 시작 검사 통과. 자동 Qt 전후 화소와 로그: `build/qa/drawing-panel-spacing/REPORT.md`. 면적 표시·시굴격자·패널의 후속 세 묶음을 각각 검증하고 마무리했다. 사용자 앱을 조작하거나 포터블을 제작하지 않았다.

## 회귀 안정화 — 2026-09-08 (이전 자동 검증 20/20, 사용자 실측 확인은 별도)

- 사용자가 “하나를 고치면 다른 기능이 고장 난다”는 회귀를 우선 해결하도록 요구했다. 하천명 표시·DEM의 지형 음영 자동 추가 제거·새 지질도 공급원 연결은 보류하고 이번 안정화 결과를 보고한 뒤 멈춘다.
- 실제 16:13 지형맵/16:01 지적 추가 종료 스택은 위성 정렬의 `QgsLayerTreeGroup::reorderGroupLayers`에 모인다. 설치 SDK의 삭제 노드 재참조 경로를 피하도록 앱의 3호출을 `LayerOps::moveLegendLayer`로 대체했고, 중복 정리 전부터 재진입을 막는다. 복제 삽입 후 기존 노드만 제거하여 원본 데이터와 레이어 수명을 보존한다.
- 주제도 100001 축척 제한을 해제했다. 기존 앱 생성 자료의 같은 제한만 열기 후 메모리에서 갱신하고, 외부 자료의 사용자 제한은 보존한다. 새 다운로드 범위/지질 80 km·수계 160 km 제한은 유지한다.
- 주소 팝업을 클릭한 모니터 작업영역 안에 배치하고 작은 화면에서는 줄바꿈한다. 일반·150% DPI 각각 QtTest 9/9 및 자동 화소 확인을 마쳤다.
- 별도 WMS 종료 스택의 부분 렌더 중첩을 완화하도록 `RenderPartialOutput`을 해제했다. 실제 로컬 XYZ의 타일 수신 중 이동·취소·최종 화소 검사(5186/5187)는 통과했지만 이전 부분 출력 설정도 같은 합성 검사에서 통과했다. 따라서 이 별도 현장 종료를 완전히 재현·해결했다고 표시하지 않는다.
- 앞선 공통 우클릭 복구, Delete 레이어 제거와 Ctrl+Z 복원, 꼭짓점·일괄 삭제 Undo, DEM/오프라인 다운로드 진행·취소, 큰 축척 분모에서 팬 수정도 현재 코드에 보존되어 전체 검사 대상이다. 병합·격자 재생성 등 모든 Undo 경로와 과거 전체 Phase가 완료된 것은 아니다.
- `scripts/verify-release.ps1`이 구성·Release 빌드·전체 CTest·headless 시작 검사 후 소스/테마/테스트/EXE 해시 기록을 남긴다. `publish-desktop.ps1`은 이 기록과 현재 입력이 일치해야 복사하며, 실행 중인 사용자 앱은 강제 종료하지 않고 배포를 거부한다. Codex 전역 hooks 설정은 변경하지 않았다.
- 최종 전체 CTest **20/20, 201.25초**, Release 시작 검사, D: portable의 시스템 PATH만 남긴 직접 EXE 시작 검사 exit 0. EXE SHA256 **4C09CC66E760304576CB5704E3680DDCF38BEA3A3F4D2F86FDA158E8D61FBA8B**. 원본 조사 파일 수정 및 커밋 없음.
- 실제 사용자 앱의 화면 자동 조작은 중단 요청을 유지하여 실행하지 않았다. 자동 Qt/QGIS 합성 캡처를 실제 현장 portable 화면 검증으로 대체하지 않는다. 보고/화소/로그: `build/qa/regression-repair/REPORT.md`. L: 이동식 포터블 배포 결과도 이 보고서에 기록한다.

## 앱 전체 테마·광택·축척 추가 — 2026-09-08 (자동 검증·배포 완료, 실제 화면 확인 미완료)

- 사용자의 후속 요청에 따라 버튼 비례에 이어 **앱 전체 테마·아이콘을 실제로 변경**했고, 가장 최근 요청인 **10000·25000 축척 추가, UI 색 20% 완화, 광택 강화**까지 구현했다. 앞선 단계 순서만을 이유로 명시적인 후속 요청을 보류하지 않는다.
- 크림 바탕을 쿨그레이/차콜/청록빛 파랑의 공통 팔레트로 통일했다. 수계는 파랑, 토양은 갈색·황토층과 검은 윤곽이며 기능별 아이콘 색과 상태가 구분된다. 리본·홈·레이어/파일함·메뉴·입력·상태표시줄·지도/조판/사진 정합의 UI 바탕에 같은 테마를 적용했다. 지도 심벌과 PDF 용지 내용 색은 보존한다.
- 최신 완화 정책: 기능 채움색의 HSL 채도는 직전의 80%, 밝은 표면은 기존 RGB 80%+흰색 20%. 글자·윤곽·어두운 홈 레일은 가독성을 위해 보존한다. 상단 반사광 띠와 중간톤/하단 음영을 강화했다. 팔레트 계산 최저 대비 4.821:1, 실제 자동 위젯의 검사한 상태 중 최저 5.251:1.
- 도면 정보는 기존 3열 정렬에 10000/25000을 한 행 추가했다. `syncScaleChips()`가 실제 QToolButton을 찾게 고쳐 선택 표시가 하나만 남는다. 합성 EPSG:5187 스튜디오에서 버튼 클릭→지도 축척·입력값·선택 상태를 검사한다.
- 전체 Release 빌드, 테마 21개 검사, build/portable 시작 스모크와 배포가 통과했다. 첫 전체 CTest의 workflow QSS 검사는 고정 220자 제한 때문에 실패해 해당 규칙의 닫는 중괄호까지 확인하도록 고쳤다. 캡처 fixture의 한글 글꼴·실제 테마 적용도 보완했다. 수정 후 최종 전체 CTest **17/17(179.27초)**가 통과했다. 테마 21개와 실제 축척 버튼 단독 검사도 통과했다.
- 배포 실행 파일 SHA256은 `D1B11AD5FE89580F62B4EF76925996937EB415D0DEA62333FE5A34FC8DA9592B`이며 Release와 portable가 동일하다. 소스/portable QSS도 동일하다. `publish-desktop.ps1` 실행 직전 HGIS 프로세스가 없음을 확인했고 사용자 앱을 강제 종료하지 않았다.
- **실제 portable 변경 후 화면·PDF 검증은 미완료**다. native 창 목록 조회에서 사용자 물리 Escape 중단이 반환되어 이번 턴에 추가 Computer Use를 하지 않는다. 자동 Qt 렌더와 기존 사용자 캡처를 실제 portable 변경 후 검증으로 대신하지 않는다. 다음 단계 자동 진행과 전체 요구 완료 판정은 하지 않는다.
- 최신 상세 보고·팔레트·전후 자동 화소·검증 로그: `build/qa/pro-ui-soft20/REPORT.md`. 이전 전체 테마/아이콘 근거는 `build/qa/pro-ui-step2/`, 버튼 비례 기준은 `build/qa/pro-ui-step1/`. 사용자 원본 조사 파일을 수정하지 않았고 커밋하지 않았다.

## 고고학 전용화 1단계 — 2026-09-08 (검증 미완료)

- 이전 요청의 첫 단계인 레이어별 우클릭 메뉴를 구현했다. 2 UI/동선/Undo, 3 격자, 4 조판, 5 전체 GIS 정리는 아직 완료하지 않았다. 현재 우선순위는 위의 전문가용 UI 요청이다.
- `MainWindowContextMenus.cpp`에 조사 layer_key별 업무 순서와 참조/외부 자료 메뉴를 구성했다. 적용 불가 기능은 제외하고 일시적 불가 기능에는 비활성 이유를 붙인다. 목록 제거는 도형 삭제와 분리하고, 아래로 이동은 QGIS registry의 지연 삭제를 피한다.
- 실제 사용자 Esc로 화면 조작이 중단됐다. 변경 전 캡처 8개만 저장됐으며 변경 후 portable 9종과 지도 메뉴 검증은 미완료다. 사용자 조사 앱은 사용자가 닫았고 강제 종료하지 않았다.
- 우클릭 후 창 종료 시 지도 도구의 파괴 순서 문제를 수정했다. save_open_window와 workflow는 전체 CTest에서 통과했으며 최종 검사 로그는 UI 단계에 함께 기록한다. 상세 결과와 재개 지점은 `build/qa/archaeology-step1/REPORT.md`, `.codex/NOW.md`에 있다. 메뉴 화면 검증 완료로 취급하지 않는다.

## Phase 1 안정화 — 2026-09-08

- 새 조사와 다른 이름으로 저장에서 기존 동명 GPKG/QGZ를 보호한다. 저장 실패 시 미저장 상태와 원래 레이어 소스를 유지하며, QGZ 사본 실패는 GPKG 저장 결과와 구분해서 알린다.
- 조사 전환 전 저장/버리기/취소를 확인하고, 닫기 저장은 메모리 벡터도 조사 파일에 보관한다. 저장·열기 중 창 닫기 재진입은 차단한다.
- 토양·지질·수계 및 고지형의 토양 준비는 취소 가능한 QgsTask에서 수행한다. 위치/행정경계는 20초 전체 제한, 참조지도 요청은 각 15초 제한을 적용하며 늦은 응답을 다른 조사에 적용하지 않는다.
- 설치 QGIS의 공통 임시 ZIP 충돌을 피하도록 앱/테스트의 임시 폴더를 분리한다. 앱이 생성한 임시 래스터의 기존 저장 참조를 보존하기 위해 앱 임시 폴더를 종료 시 자동 삭제하지 않는다.
- 꼭짓점 이동 실패 시 편집을 유지하고, 화면/레이어 CRS가 다른 경우에도 좌표를 올바르게 변환한다.
- Release 빌드, 전체 CTest 17/17(152.32초), 스모크와 portable 배포 통과. 실제 화면·재측정과 상세 변경은 `build/qa/production-phase1/`에 기록한다. Phase 2 이후는 이번 Phase 보고 뒤 멈춘다. 원본 현장 자료와 제출 규격, 지질 80 km/수계 160 km 범위는 유지한다. 커밋하지 않았다.

## Resume after reconnect (same PC or clone)

이 PC 작업본: `D:\hgis` · OSGeo **`D:\OSGeo4W`**. 세션 재개 시 `.codex/NOW.md`부터.

Remote: `https://github.com/kwonyoungin11/hgis.git`

```powershell
cd "D:\hgis"
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
.\scripts\run-ka-hgis.ps1
```

Launch for the field user (do **not** start `ka-hgis.exe` raw):

- **시작 동작:** 홈 화면만 연다. 최근 프로젝트·도면·배경지도를 자동으로 불러오지 않는다. 사용자가 **조사 열기** 또는 최근 조사 항목을 선택해야 프로젝트를 연다.

- Desktop **고고학 전용 HGIS.lnk** → `dist\ka-hgis-portable\start.bat`
- After C++/UI builds: `.\scripts\publish-desktop.ps1` then click that icon
- or `.\scripts\start-ka-hgis.vbs` → `launch.ps1` (uses `build\Release` first)

Other PC first time: `docs/other-pc-setup.md` · `.\scripts\bootstrap-dev-pc.ps1`  
VWorld key is **local only** (`VworldSettings` / 도움말 → API 키). Never commit `config/secrets.ini`.
Key stores, in the order `loadApiKey()` reads them — a stale entry in an earlier one hides the rest:

1. `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` — SSOT, what the app writes (**not** `%APPDATA%`)
2. `HKCU\Software\ka-hgis\ka-hgis` (NativeFormat), then env `VWORLD_API_KEY`
3. `config/secrets.ini` — **the place to put a dev key**; gitignored (`**/secrets.ini`)

Saved surveys bake the key into the layer URL (`req/wmts/1.0.0/<KEY>/`, `KEY=<KEY>`), so a renewed key
does nothing for an old file until `LayerOps::refreshVworldApiKeyInLayers` swaps it on open
(`ensureDefaultBasemaps`). An expired key answers **HTTP 200 with an XML error**, not an HTTP error —
tiles just come out blank with nothing in the log.

---

## 1. Product

Korean field archaeology HGIS (C++20/Qt6 + OSGeo4W `qgis-dev`, Architecture B, no QGIS fork):

1. GPKG survey store; **legend empty until draw/import** (`LayerOps::ensureDomainLayer` only). **새 조사** drops every non-basemap layer (keep WMS/XYZ 지적·위성 only)
2. Domain keys: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`
3. **참조 지도** (위성/지적) vs **조사 데이터**
4. Digitize: startEditing → addFeature → commit (keep tool). After a polygon is finished, drag a saved vertex to reshape. **Ctrl+Z** undoes last vertex, then last saved feature. **No autosave** — `persistSurveyWork` runs only from `저장` (Ctrl+S) and from the close prompt (저장 / 저장 안 함 / 취소, `surveyHasUnsavedChanges`). Unsaved work shows as ` *` after the window title. `저장` also brings external vectors into the survey GPKG. Both commit edits before saving the embedded workspace. Failed commits retain the edit buffer and stop workspace saving. No QGZ dialog on 저장. 새 조사·다른 이름으로 저장 start in `preferredSurveyDir()` (last survey folder), never the Desktop — a Desktop redirected into OneDrive is what put field data there.
4a. **저장·다시 열기**: the GPKG contains survey vectors and an embedded QGIS workspace; the companion QGZ is a secondary copy. Reopening restores missing legend nodes while preserving existing visibility, names and styles. **다른 이름으로 저장** includes committed SQLite WAL changes and keeps each layer's actual table name (including `survey_area_2`). External photos/rasters still require their source files. A failed workspace restore suppresses automatic workspace overwrite and routes explicit saving to a different file.
5. **도면 만들기** = `KaDrawingStudio` (not QGIS Layout Designer). Samples for north/scale/legend/CRS. **Ctrl+Z** removes last placed item. 좌표점은 지도 칸만, 우클릭/Delete/Ctrl+Z로 마지막 점 삭제. 축척·이동 후 땅 XY에서 다시 앉힘. 자석은 조사 벡터 꼭짓점·선(지적 WMS 그림은 불가). 전문 도곽(+ 십자·테두리 좌표 자)은 **격자 설정에서 켤 때만**. 도면만들기 기본·PDF는 맵 테두리만(축척자·방위·CRS는 유지). 자동 도면(`fillLayout`)은 표제란(도면명·조사명·축척·좌표계·작성일) 포함. 도면의 래스터(위성·지적·지질)는 조각 렌더 없이 한 번에 그린다(`LayoutService::applySingleRasterPassRendering`) — QGIS 기본 조각 렌더는 조각 하나가 비면 위성이 반만 나온 것처럼 보인다
5c. **단면도** = `KaSectionDrawingStudio` 전용 탭. 열면 A3/A4 용지와 표고·거리 눈금이 이미 있다. 좌측 **GeoTIFF 추가**만 쓰고 위성·지적·조사 벡터는 목록에 넣지 않는다. CRS는 EPSG:5187/5186 선택(재투영 없음, 거리×표고 m). PDF는 `SectionLayoutService::exportSectionPdf`(300 DPI, forceVector, AlwaysText)
5b. **맞추기** = same-canvas JPG/DXF onto 지적 (Fit To Display + 2–3 pairs). Result is **참조 지도**, not submit geometry. Not GNSS `control_points`. **이동**은 같은 `QgsRasterLayer`에 월드파일만 적용(`persistAlignedRaster`). `removeMapLayer`+재오픈·`refreshAllLayers`/`zoomToLayerMax` 금지(지적 WMS 그리는 중 멈춤). 화살표/번호는 화면 캐시가 아니라 매번 `mapX/mapY`에서 다시 찍는다. 흰 종이 스캔만 먹선 Multiply. **항공 GeoTIFF는 원색 SourceOver**(대비·감마·흰 픽셀 투명을 주지 않음).
5d. **고지형** = `PaleoLandformService`. 흙토람 04/05/06/08 강조 + `seedInterpretationFromSoil` 가설 자동 깔기(04 선상지·05 해성평탄·08 하안단구·06 구하도/자연제방 또는 좁으면 미저지). `note` `자동:`만 재실행 시 교체. 토양 글자는 큰 입지후보만. **참조 지도**, not domain, not 5179. Not palaeo-DEM / 입체시 / 복원 주장 금지.
5g. **토양도** = WFS `SOIL_1/2/3`(분포지형 속성·고지형) 아래 흙토람과 같은 `TOP_A_SOIL_T_GEO` 3857 그림(산능선 산악지). 목록에 벡터+그림 두 줄. WMS `crs=`에 5186 금지. 제출 도메인 아님. 조판 범례는 레이어 체크와 같다(`tuneSheetLegend`). 끈 레이어·그림·위성·지적 이름은 범례에 넣지 않음.
5h. **지질도 입체** = `GeologyMapService::ensureReliefUnderlay`. 지질 색(KIGAM 1:5만) **위**에 `지질 음영`(로컬 DEM hillshade 또는 Esri World_Hillshade XYZ). 음영 Multiply·opacity 0.48, 지질 색 SourceOver. 산이 솟는 3D 렌더 아님. **참조 지도**. 조판 범례에서 음영 이름 생략. 다시 누르면 지질+음영 같이 숨김.
5i. **입체지형** = 리본 진입 **삭제**(사용자 포기). 엔진/`terrain3d_sheet` 코드는 남김. 제출 5179 아님.
5e. **레이어 글자** = 벡터 우클릭 「글자 끄기/켜기」(`LayerOps::setLabelsVisible`). 지적 WMS의 지번 글자는 그림에 박혀 있어 레이어를 통째로 꺼야 함.
5f. **시굴격자 비율** = 조사구역 레이어 우클릭 → 시굴격자 → 시굴(10%) / 표본(2%). 폭 2 m 고정, 길이·간격 배분(`buildForTargetRatio`). 선택한(없으면 마지막) 구역만.
6. Work CRS default **EPSG:5187 (동부)**; 5186 also OK. **export SHP+PDF+MANIFEST = EPSG:5179**. 리본 「인트라넷 내보내기 5179」/「5179변환」은 파일만 쓰고 지도·범례에 올리지 않는다. Checklist error hard-blocks 제출
7a. **축척 칸은 하나** — 편집 가능한 `scaleCombo` 하나뿐(입력줄이 그 안의 `scaleEdit`). 예전의 별도 QLineEdit + 프리셋 QComboBox 두 개 구성으로 되돌리지 말 것. `MainWindow::scaleDenominatorFromUi` 가 "2000"·"1:2000" 을 모두 읽는다. 프리셋은 1:100 부터.
7. 초보자 리본 6그룹(조사파일/기록/배경/정합·분석/산출/찾기). 질문형 짧은 라벨. Text menu bar **hidden**. **파일함은 기본 표시**(더보기로 숨길 수 있음). 작업 제어 dock은 더보기. 조판 위 「보기」 리본 없음 — 줌·이동은 마우스, 용지 맞춤은 열 때 자동.

---

## 2. What landed (do not rebuild)

| Area | Notes |
| --- | --- |
| VWorld 위성 | Stored API key → `api.vworld.kr` WMTS **first**; xdworld only if no key |
| VWorld 지적 | Frozen tiled WMS `crs=EPSG:3857` + KEY. **`DOMAIN` 은 보내지 않는다** — 붙이면 같은 키·같은 Referer 라도 `INCORRECT_KEY`. 인증은 Referer 헤더가 한다(`KaApplication` requestPreprocessor + `http-header:referer`). 같은 이유로 수계도 WFS 에서도 뺐다. Do not put 5186/5187/5179 in WMS CRS list. GDAL_WMS 설정은 `%LOCALAPPDATA%\ka-hgisworld-cadastral.xml` (임시폴더 아님 — 비워지면 레이어가 깨졌다) |
| Digitize / attrs | `KaCaptureMapTool`, `KaAttributeMapTool`, `ensureDomainLayer`. 그리기: 조사구역/유구면/유구선/단면선/기준점. 레이어 삭제·그린 도형 삭제는 `purgeCommittedFeatures`(GPKG 비움). 도형수정 자석=`snapToMap`, 선 우클릭 점추가/점삭제. 시굴격자 자동배치는 **선택한(없으면 마지막) survey_area만** — leftover union 금지. 자석은 벡터 꼭짓점·선만(지적 WMS 그림에는 안 붙음) |
| Layout studio | `src/app/KaDrawingStudio.*` — 160 mm scale bar, PNG north = sample, CRS label |
| Section studio | `src/app/KaSectionDrawingStudio.*` + `src/core/SectionLayoutService.*` — A3/A4 landscape, GeoTIFF body, elevation/distance ticks, vector PDF |
| Terrain 3D tab | `src/app/KaTerrain3dStudio.*` + `src/core/Terrain3dService.*` — 전용 **입체지형** 탭. **지금 지도 화면**을 고해상 메시+Google 위성으로. DEM 파일 열기는 보조. `qgis_3d` 없음 |
| Launch | `scripts/start-ka-hgis.vbs` + `launch.ps1` (Job Object safe) |
| Chrome theme | `KaTheme` + `data/theme/ka-hgis.qss` sky 3D / black 2px regions |
| Tests | `tests/test_workflow.cpp` (satellite key-first, undo feature, scale bar width, …) |

---

## 3. Next (if continuing production)

1. Export package PDF should be the **composed studio sheet**, not `rebuildDefaultLayouts` 5 templates
2. Checklist `layout_exists:*` must not pass on empty auto-seeded layouts
3. Do **not**: QGIS fork, DXF submit, restore 7-step rail, change cadastral WMS recipe, hardcode VWorld key

---

## 4. Layout / build

```
src/app/MainWindow.*        chrome, digitize, export
src/app/KaDrawingStudio.*   조판
src/app/KaSectionDrawingStudio.* 단면도 탭
src/core/SectionLayoutService.* 단면 눈금·조판·PDF
src/app/KaCaptureMapTool.*  그리기
src/app/KaAttributeMapTool.* 속성 클릭
src/core/LayerOps.*         basemap, layer_key, undoCommittedFeature
src/core/PaleoLandformService.* 고지형 1단계 (참조 판독)
src/core/ExportService.*    SHP 5179 + MANIFEST
src/core/LayoutService.*    default 5 layouts (package PDF still uses these)
src/core/ChecklistEngine.*
tests/test_workflow.cpp
```

```
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
.\scripts\run-ka-hgis.ps1
```

This machine: OSGeo `D:\OSGeo4W` (dev-env also accepts C: / D:). CMake `C:\CMake\bin`.

---

## 5. Codex Native

| Surface | Path |
| --- | --- |
| Agent rules | `AGENTS.md` |
| Session now | `.codex/NOW.md` |
| Product SSOT | `HANDOFF.md` + `docs/HANDOFF.md` |
| Build/search tools | native Codex tools, PowerShell, `rg`, CMake, ctest |
| C++ language tooling | `.clangd`, `.clang-format`, `.clang-tidy`, `CMakePresets.json` when present |
| Verification scripts | `scripts\dev-env.ps1`, `scripts\run-ka-hgis.ps1`, `scripts\publish-desktop.ps1`, `scripts\build-all.ps1` |
| Legacy compatibility | `.grok`, `.cursor`, `.agents`, and `orca.yaml` may exist but are not required Codex presets |
