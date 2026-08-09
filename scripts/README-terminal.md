# 터미널 / 실행 시 보이는 “에러” 안내

## GRASS 플러그인 메시지 (무시해도 됨)

```
plugin_grass8.dll: ...invalid (lib not loadable)
provider_grass8.dll: ...
```

- **원인:** OSGeo4W qgis-dev에 GRASS 플러그인이 있으나 GRASS 런타임이 없음
- **영향:** 없음. ka-hgis는 GRASS 없이 동작
- **exit code:** 0 이면 성공

PowerShell 5.1은 이 메시지를 stderr로 받아 빨간 `NativeCommandError`처럼 보여 줍니다.  
`run-ka-hgis.ps1`은 이를 **실패로 처리하지 않도록** 수정되어 있습니다.

## 올바른 실행

```powershell
cd D:\qgis
.\scripts\dev-env.ps1
.\scripts\run-ka-hgis.ps1          # GUI
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\build-all.ps1
.\scripts\e2e-smoke.ps1
```

## 진짜 실패인 경우

- `ka-hgis.exe not found` → 먼저 빌드
- smoke exit ≠ 0 → DLL PATH / `pdal-dev\bin` 확인 (`dev-env.ps1`)
- ctest FAIL → `ctest --test-dir build -C Release --output-on-failure`
