# 배포 (Windows)

## 권장 모델

1. 대상 PC에 OSGeo4W 설치 (`qgis-dev` 등 — DLL은 앱과 함께 묶지 않음)
2. `scripts/dev-env.ps1` 또는 포터블 폴더의 `dev-env.ps1`로 PATH/prefix 설정
3. `ka-hgis.exe` 실행

다른 PC 전체 가이드: [`other-pc-setup.md`](./other-pc-setup.md)

## 런처

- 개발: `scripts/run-ka-hgis.ps1`, `run-ka-hgis.bat`
- 포터블: `dist/ka-hgis-portable/run.bat`

포터블 생성: 빌드 후 `.\scripts\make-portable.ps1` (또는 `build-all.ps1`에 포함)

## 주의

- `qgis_core.dll` 등 다수 DLL 필요 → 단일 exe 정적 링크 비권장
- MSVC 런타임과 QGIS(OSGeo4W) 빌드 일치 필요
- VWorld API 키는 사용자 PC 로컬 설정 — 바이너리에 넣지 않음
