# Design

## Source of truth
- Status: **Active** (first canonical pass)
- Last refreshed: 2026-08-07
- Primary product surfaces:
  - Desktop main window (`src/app/MainWindow.*`) — single-screen field HGIS
  - Print layouts / PDF sheets (`src/core/LayoutService.*`)
  - App icon set (`icon/ka-hgis-*`)
- Evidence reviewed:
  - `docs/ux/ia-beginner.md`, `docs/ux/mainwindow-wireframe.md`
  - `docs/user/job-cards/*`, `docs/user/gui-scenario-checklist.md`
  - `docs/research/01-kr-excavation-drawings.md`, `docs/adr/0001-standalone-cpp-qgis-libs.md`
  - `README.md`, `src/app/MainWindow.cpp`, `src/app/KaIcons.cpp`
  - `icon/ka-hgis-icon.svg`, `icon/README.md`
  - Runtime screenshots / a11y tree (prior sessions): step rail, dual toolbars, guide panel, map canvas

## Brand
- Personality: **현장 신뢰 · 차분한 전문성 · 흙/지도** (문화재 조사 도구, 장난감 UI 아님)
- Trust signals: 한국어 1차, 법령형 검수 문구, CRS/측량 용어 명시, 제출 패키지·조판(범례·축척·방위)
- Avoid:
  - 일반 QGIS/ArcGIS 메뉴 정글 노출
  - 네온/게임형 과한 그라데이션 UI
  - 영문 GIS 은어만 있는 버튼
  - “예쁜 지도 앱” 톤 (현장 제출 도구가 우선)

## Product goals
- Goals:
  - 초보 조사원이 **7단계**로 조사→작도→검수→제출까지 완주
  - 문화재 GIS 전문가 기준: 실측 폴리곤, GCP≥2, 작업 CRS 5186/5187, 업로드 5179
  - ArcGIS 의존 없이 독립 exe + QGIS libs
- Non-goals:
  - 풀 QGIS 클론, Mesh/3D/GRASS 노출, 인트라넷 자동 업로드, 사진측량급 지오레퍼
- Success signals:
  - 첫 실행 후 가이드만으로 최소 제출 플로우 완료
  - 검수 error 0 → SHP+PDF+MANIFEST
  - 초보자가 CRS 작업/업로드 구분을 이해

## Personas and jobs
- Primary personas:
  - **현장 초보 조사원** (ArcGIS 약함, 한글·단계 필요)
  - **조사 책임/도면 검수자** (법령·제출 형식)
  - (2차) 문화재 GIS 담당 (CRS·SHP·조판 품질)
- User jobs:
  - 새 조사 GPKG 생성 → 지적/지형 중첩 → 구역·유구 작도 → GCP → 검수 → 5179·PDF·제출
- Key contexts of use:
  - Windows 노트북, 현장/사무실, 종종 약한 네트워크(타일 배경)
  - 터치보다 마우스·키보드

## Information architecture
- Primary navigation:
  - **왼쪽 7단계 레일** (주 항해)
  - 상단: 위치검색 툴바 + 주요 아이콘 툴바
  - 메뉴: 파일 / 좌표계 / 배경지도 / 도구 / 도움말
- Core routes/screens:
  - 단일 MainWindow (라우트 없음)
  - 단계별 도구 스트립 + 지도 + 레이어 트리 + 오른쪽 「지금 할 일」
- Content hierarchy:
  1. 지금 할 일 (가이드)
  2. 지도·작도
  3. 레이어
  4. 검수 결과
  5. 전문가 메뉴(좌표계 위험 옵션 등)

## Design principles
- Principle 1: **한 화면에 한 다음 행동** — 「지금 할 일」+「다음 단계」가 주 안내
- Principle 2: **전문 규칙은 숨기지 말고 쉽게** — 종류/시대 필수, 폴리곤만, GCP 2+
- Principle 3: **작업 CRS ≠ 업로드 CRS** 를 UI 카피로 반복
- Tradeoffs:
  - 아이콘 밀도 높음(툴바) vs 초보 단순함 → 단계 레일이 주, 툴바는 단축
  - QGIS 기본 룩 vs 브랜드 톤 → 현재 Qt 기본 + 색 아이콘; 토큰 시스템 없음

## Visual language
- Color:
  - App icon: teal `#0F3D4C`→`#1F6F78` + earth `#8B5A2B` / soil gold
  - UI accents (KaIcons / guide): blue `#2563eb`, orange draw, pink line, teal GPS, green check, red delete/PDF
  - Canvas: light gray-blue `#f5f7fa`
  - Guide panel: `#eff6ff` border `#93c5fd` text `#1e3a8a`
- Typography:
  - UI: system Qt (Windows → Segoe UI 계열)
  - Layout/PDF: **Malgun Gothic** 명시
- Spacing/layout rhythm:
  - 3-column: rail ~230px | map flex | guide ~260px
  - Step row height ~44px, toolbar icon 28px
- Shape/radius/elevation:
  - Guide/button: ~6–8px radius (inline stylesheet)
  - No global elevation system
- Motion:
  - Map pan/zoom, tile load; no branded motion language
  - Prefer reduced flicker (canvas cache / update interval)
- Imagery/iconography:
  - Drawn `KaIcons` (not QGIS theme pack)
  - Dedicated draw icons: `draw_area`, `draw_poly`, `draw_line`
  - App icon assets in `icon/` (not fully wired to exe window icon yet)

## Components
- Existing components to reuse:
  - Step rail `QListWidget#stepRail`
  - Step tools `QToolBar#stepTools`
  - Main toolbar `#mainToolbar`, search `#searchToolbar` / `#locationSearch`
  - Layer tree + Delete, map canvas, guide `#guideNow`, next `#btnNextStep`
  - Checklist view `#checkView`, help `#helpPanel`
  - Dialogs: new survey CRS pick, feature kind/period, GCP form
- New/changed components (recommended):
  - Wire `icon/ka-hgis.ico` to application/window icon
  - Step completion checkmarks on rail
  - Single “primary CTA” per step (reduce toolbar competition)
  - Unified stylesheet / token file instead of scattered inline styles
- Variants and states:
  - Step selected vs idle
  - Capture mode (crosshair) vs pan
  - Checklist pass (green) / error (red) / warn (orange)
- Token/component ownership:
  - UI chrome: `MainWindow` + `KaIcons`
  - Map/layout: QGIS widgets + `LayoutService`

## Accessibility
- Target standard: **WCAG 2.1 AA intent** (desktop Qt; not formally audited)
- Keyboard/focus:
  - Delete/Backspace layer remove; capture Enter/ESC/Backspace
  - Search Return; need visible focus rings audit
- Contrast/readability:
  - Guide blue-on-blue-tint OK; toolbar text-under-icon small on dense bars — watch low-vision
- Screen-reader semantics:
  - Limited (custom canvas); rely on labels/objectNames for automation
- Reduced motion:
  - No decorative animation; map refresh is functional

## Responsive behavior
- Supported breakpoints/devices: **Windows desktop ≥ ~1280×800** (designed 1440×900)
- Layout adaptations: fixed side columns; map takes remainder (no mobile layout)
- Touch/hover: mouse-first; touch not a design target

## Interaction states
- Loading: status bar messages (“검색 중…”, basemap); no global spinner pattern
- Empty: empty map + guide step 1; checklist “검수 결과 없음”
- Error: `QMessageBox` + red checklist HTML; FeatureWriteService Korean errors
- Success: status bar green-path messages; checklist all pass
- Disabled: (gap) future steps not hard-locked yet despite wireframe note
- Offline/slow network: basemap/search fail messages; local GPKG still works

## Content voice
- Tone: **존중하는 현장 한국어**, 짧은 명령형 + 이유 한 줄
- Terminology:
  - 조사 / 유구 / 기준점(GCP) / 검수 / 제출
  - 작업 CRS(5186·5187) vs 업로드(5179)
  - ArcGIS 용어는 job-card 별도 (`docs/user/job-cards/arcgis-용어.md`)
- Microcopy rules:
  - 위험 동작에 “(위험)” 표기 (CRS 이름만 지정)
  - 필수 속성은 대화상자에 예시 포함
  - 상태바는 다음 행동으로 끝맺기 (“→ 저장 누르세요”)

## Implementation constraints
- Framework/styling system: **Qt6 Widgets + QGIS GUI**, no web CSS framework
- Design-token constraints: none yet — introduce only if shared QSS/tokens file
- Performance constraints: tile basemap flicker control; avoid full canvas clear storms
- Compatibility: Windows + OSGeo4W qgis-dev runtime
- Test/screenshot expectations:
  - Domain TDD: ctest domain_tdd / workflow / checklist
  - Manual: `docs/user/gui-scenario-checklist.md`
  - Visual regression: not established (optional `$visual-ralph` later)

## Design verdict (2026-08-07)

### What is already good
- **IA matches the job**: 7-step rail + job cards is the right beginner model for excavation drawing.
- **Expert rules surface gently**: kind/period, polygon-only, GCP meta, 5179 upload path.
- **Guide panel + next button** align with “one next action.”
- **Icon language** separates draw tools from generic GIS actions; brand icon has coherent earth/GIS story.
- **Print design** includes title, north, legend, scale — submission-minded.

### What is not good enough yet
- **No single DESIGN contract before now** — UI grew feature-by-feature; density and doc drift (README still says default CRS 5179; product work CRS is 5186/5187).
- **Chrome density**: main toolbar packs many peers; competes with step tools (cognitive load for true beginners).
- **Visual system incomplete**: mixed inline styles, Qt default menus, app icon not wired, no step-complete affordance, future steps not locked.
- **Docs vs UI drift**: wireframe CRS 5179; ia-beginner step1 5179; implementation 5186/5187 work + 5179 upload.
- **States**: weak loading/empty polish; checklist HTML basic; no progressive disclosure for expert CRS dangers beyond menu labels.

### Overall score (product design, not pixel polish)
- **Structure / IA:** Good (B+)
- **Beginner guidance:** Fair–Good (B)
- **Visual cohesion:** Fair (C+)
- **Expert correctness in UX copy:** Good if CRS story is consistent (B, after doc fix)
- **Ship-ready design system:** Not yet (C)

**Bottom line:**  
**업무 구조(7단계·검수·제출)는 방향이 맞고 “괜찮다”에 가깝다.**  
**시각·밀도·문서 일관성·단계 잠금은 아직 미완이라 “완성된 디자인”은 아니다.**  
초보 문화재 GIS 도구로 가려면 **툴바 단순화 + CRS 카피 통일 + 단계 완료/잠금 + DESIGN.md 준수**가 다음 디자인 작업이다.

## Open questions
- [ ] 현장 기본 모니터 해상도/멀티모니터? (owner: product, impact: layout density)
- [ ] 기관 브랜드 색/로고 필수 여부? (owner: stakeholder)
- [ ] 초보 모드에서 상단 툴바를 숨기고 단계 CTA만 둘지? (owner: UX)
- [ ] 접근성 공식 목표 WCAG AA 강제 여부? (owner: product)
- [ ] 시각 레퍼런스(스크린샷 골든) 둘지 → 이후 `$visual-ralph` (owner: design)
