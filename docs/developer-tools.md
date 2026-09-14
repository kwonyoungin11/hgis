# HGIS 개발 도구

현재 PC의 저장소는 `A:/qgis`, SDK는 `A:/OSGeo4W`다. 도구별로 실제 검증된 작업을 맡기고, 원본 코드와 컴파일러·테스트로 결론을 확인한다.

| 도구 | 실제 개발에서 사용 | 범위 |
| --- | --- | --- |
| clangd + 실제 컴파일 DB | 진단, 호출 위치의 선언·정의 확인 | [탐색 명령](clangd-navigation.md), 표본 정의 조회 10/10 |
| Graft MCP | 후보 정의 검색, 파일 구조, 인덱싱된 파일의 문자열 검색 | `src/`, `tests/`; 전체 호출/영향 분석 제외 |
| Archify | 소스로 확인한 구조를 독립 HTML로 검토 | [설치·실행](archify-setup.md), 구조도 품질·브라우저 검증 |
| CMake / CTest | Release 컴파일과 동작 검증 | 기존 프로젝트 스크립트; 도구 결과로 대체하지 않음 |

## Graft 설치와 재현

검증한 upstream은 [NanoNets/Graft](https://github.com/NanoNets/Graft/tree/f9e65396e638e517aecae0d731017f53084d70ed), 버전 표기 `0.18.0`, 고정 커밋 `f9e65396e638e517aecae0d731017f53084d70ed`다. npm 최신 패키지와 동일한 소스라는 뜻은 아니다.

```powershell
.\scripts\setup-graft.ps1
node --test scripts/graft-mcp.test.mjs
# 설치된 의존성/생성 파일 손상 시 고정 소스를 유지한 재빌드
# .\scripts\setup-graft.ps1 -Rebuild
```

Git, Node.js/npm, Python, VS C++ 도구가 필요하다. 설치 스크립트는 `build/tooling/graft-source`에 고정 소스를 받아 lockfile 의존성을 설치하고 필요한 native parser만 빌드한다. 기존 소스가 다른 커밋이거나 수정되어 있으면 중단한다. 인덱스는 `build/tooling/graft-index`에 생성한다. 재실행은 설치된 빌드를 재사용하며 인덱스를 새로 만든다. 두 폴더 모두 기존 build ignore 정책에 포함된다.

`.codex/config.toml`이 프로젝트 서버 `hgis_graft`를 등록한다. 이 PC의 Node와 스크립트 절대 경로를 사용한다. 다른 PC로 옮길 때 command/args/cwd를 새 위치로 바꾸고 설치 스크립트를 실행한다. 전역 설정·모델 선택은 바꾸지 않는다. Codex 실제 `config/read(cwd=A:/qgis)`에서 프로젝트 설정이 병합되는지 검증한다. 이미 진행 중인 턴의 도구 목록 갱신은 별개이므로 다음 턴에 서버가 보이지 않으면 Codex 앱을 다시 열어 로드한다.

서버는 `scripts/graft-mcp.mjs`를 실행한다. upstream MCP 조회 함수와 동일한 줄 단위 JSON-RPC 전송 방식을 사용하며 네 가지 도구만 노출한다.

- `graft_find_code`: 관련 정의 후보를 좁힌다.
- `graft_file_api`: 한 파일의 정의 목록을 살핀다.
- `graft_find_all`: 인덱싱된 소스/테스트에서 문자열을 찾는다.
- `graft_check_freshness`: 필요하면 구조 인덱스를 갱신하고 현재 파일 지문과 일치하는지 확인한다.

조회 전에 내용 해시까지 비교하여 구조 인덱스를 갱신하고, 갱신 실패·누락·조회 중 소스 변경은 오류로 반환한다. 생성물은 로컬 build 아래에만 쓴다. 최초 설치·의존성 다운로드에는 네트워크를 쓰지만 이후 구조 인덱싱과 조회에는 모델 API를 쓰지 않는다. upstream 자동 초기화, 자동 업데이트, 전역 hook, 토큰 절감 홍보 문구는 연결하지 않는다.

Graft의 기본 추출은 실제 호출 표본 10개 중 1개, Windows clangd 탐지만 보정한 실험도 6개만 찾았다. 따라서 `graft_trace_calls`와 graph 기반 영향 범위 판단을 제외했다. 파일·줄 범위와 오버로드는 원본 또는 clangd로 확인한다. 검색 결과가 없다는 이유로 사용처가 없다고 판단하지 않는다.

## 작업 순서

1. `AGENTS.md`, `.codex/NOW.md`, 해당 소스/테스트를 확인한다.
2. 위치를 모르면 Graft로 후보를 찾고 `rg`와 원본으로 확인한다. 호출 의미는 clangd 정의 조회로 확인한다.
3. 작은 계획과 변경을 만든 뒤 관련 빌드/테스트를 실행한다.
4. 여러 모듈의 관계를 설명할 필요가 있으면 Archify로 현재 소스 근거를 담은 구조도를 만든다. 모든 수정마다 구조도나 그래프 검사를 강제하지 않는다.

## 검증 기록과 남은 사항

이번 설정의 실행 로그는 `build/qa/dev-tools-setup-20260914/`, 이전 전체 제품 빌드·테스트 기록은 `build/qa/tooling-validation-20260914/REPORT.md`에 있다. 로컬 QA 결과는 저장소에 커밋되지 않는다.

이전 Release 빌드와 시작 스모크는 통과했다. 전체 CTest는 43개 중 39개 통과·4개 실패였고, 그중 두 테스트는 단독 재검사에서 통과했다. 반복 재현되는 heritage_flow/WMTS 검사 실패 및 save_open의 240초 제한 근접 문제는 남아 있다. 이번 도구 설정은 제품 C++나 테스트를 수정하지 않는다.

Codex 설정 근거: [프로젝트 MCP 설정](https://learn.chatgpt.com/docs/extend/mcp?surface=cli), [프로젝트 스킬](https://learn.chatgpt.com/docs/build-skills). 실제 이 PC의 유효 설정과 실행 결과를 우선한다.
