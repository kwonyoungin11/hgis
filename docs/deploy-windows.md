# 배포 (Windows)

## 권장 모델
1. PC에 OSGeo4W 또는 QGIS LTR 설치
2. `scripts/dev-env.ps1` 로 PATH/prefix 설정
3. `ka-hgis.exe` 실행

## 런처
`scripts/run-ka-hgis.ps1` (dev-env 호출 후 exe 실행)

## 주의
- qgis_core.dll 등 다수 DLL 필요 → 단일 exe 정적 링크 비권장
- MSVC 런타임과 QGIS 빌드 일치 필요
