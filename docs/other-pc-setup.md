# 다른 PC에서 작업하기 (Windows)

이 저장소는 GitHub에 올라가 있습니다. **소스 개발**과 **실행만** 두 경로를 지원합니다.

- 원격: `https://github.com/kwonyoungin11/hgis`
- 브랜치: `main`
- DLL은 벤더링하지 않음 → **대상 PC에도 OSGeo4W(qgis-dev) 필요**

---

## A) 개발 (클론 → 빌드 → 실행)

### 1. 사전 요구 (대상 PC)

| 항목 | 권장 |
|------|------|
| OS | Windows 10/11 x64 |
| Git | 설치됨 |
| CMake | 4.x (`C:\CMake\bin` 또는 PATH) |
| 컴파일러 | **VS 2022** Build Tools (MSVC, C++ 워크로드) |
| GIS SDK | **OSGeo4W** `C:\OSGeo4W` (또는 `D:\OSGeo4W` / `OSGEO4W_ROOT`) |

OSGeo4W 패키지 (개발용):

- `qgis-dev`
- `qt6-devel`
- `gdal-dev-devel`
- `sqlite3-devel`
- `pdal-dev`

> 첫 PC와 **같은 qgis-dev 계열**을 쓰는 것이 안전합니다. 핀: 저장소 `VERSION_QGIS_PIN.txt`.

### 2. 의존성 설치 (선택, 관리자 PowerShell)

```powershell
# 저장소 클론 후
cd <클론경로>\hgis
# CMake + VS Build Tools + OSGeo4W 패키지 시도
.\scripts\install-deps.ps1
```

수동 OSGeo4W: https://download.osgeo.org/osgeo4w/v2/osgeo4w-setup.exe  
설치 루트 권장: `C:\OSGeo4W`, 패키지는 위 목록.

### 3. 클론

```powershell
cd $env:USERPROFILE\Documents   # 원하는 폴더
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
git checkout main
git pull
```

### 4. 빌드·검증

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH   # CMake 위치에 맞게
.\scripts\build-all.ps1
```

또는 수동:

```powershell
.\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
```

OSGeo가 다른 경로면:

```powershell
$env:OSGEO4W_ROOT = "D:\OSGeo4W"
.\scripts\dev-env.ps1
```

### 5. 일상 실행

```powershell
.\scripts\run-ka-hgis.ps1
# 또는
.\run-ka-hgis.bat
```

### 6. 에이전트/OpenCode (선택)

- 프로젝트 규칙: 루트 `AGENTS.md`, `OPENCODE_HANDOFF.md`
- 로컬 OpenCode가 있으면 이 폴더를 워크스페이스로 열면 됨

### 7. 커밋 훅 (선택)

```powershell
.\scripts\install-git-hooks.ps1
```

---

## B) 실행만 (포터블 폴더)

`dist\ka-hgis-portable\` 를 USB/네트워크로 복사하거나, 클론 후 그 폴더만 사용.

```text
dist\ka-hgis-portable\
  ka-hgis.exe
  run.bat
  run-ka-hgis.ps1
  dev-env.ps1
  data\...
  docs\user\...
  README.txt
```

1. 대상 PC에 **OSGeo4W + qgis-dev 런타임** 설치 (위와 동일 루트 권장)
2. `run.bat` 실행

개발 없이 최신 exe만 쓰려면: 개발 PC에서 `.\scripts\build-all.ps1` 후 `dist\ka-hgis-portable` 통째 복사.

---

## 자주 막히는 것

| 증상 | 조치 |
|------|------|
| `OSGEO4W_ROOT not found` | OSGeo4W 설치 또는 `$env:OSGEO4W_ROOT` 설정 |
| `qgis-dev missing` | OSGeo4W에서 `qgis-dev` 패키지 설치 |
| DLL 없음 / 즉시 종료 | `.\scripts\dev-env.ps1` 후 실행; PATH에 `pdal-dev\bin` 포함 확인 |
| 링크/런타임 크래시 | VS2022 MSVC와 OSGeo4W 빌드 툴체인 불일치 — Build Tools 재설치 |
| CMake 없음 | `winget install Kitware.CMake` 또는 `C:\CMake\bin` PATH |
| VWorld 배경 안 됨 | 앱 **도움말 → VWorld API 키 설정** (키는 PC마다 로컬 저장, 커밋 금지) |

---

## 두 PC 동기화

```powershell
# 작업 시작
git pull origin main

# 작업 끝 (커밋 후)
git push origin main
```

필드 데이터(`*.gpkg` 등)는 `.gitignore` 대상이라 **저장소에 안 올라갑니다**.  
조사 파일은 OneDrive/NAS 등으로 따로 옮기세요.

시크릿: `config/secrets.ini`, VWorld 키 — 커밋하지 마세요.
