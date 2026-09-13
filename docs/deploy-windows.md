# 배포 (Windows)

## 권장 모델 (실행만)

1. 개발 PC에서 Release 빌드 후 `.\scripts\make-portable.ps1`
2. `dist\ka-hgis-portable\` **폴더 전체**를 USB/공유로 복사
3. 대상 Windows 10/11 64비트에서 `start.bat` 또는 `ka-hgis.exe`
4. OSGeo4W / Visual Studio 설치 불필요

다른 PC에서 **소스 개발**하려면: [`other-pc-setup.md`](./other-pc-setup.md)

## 런처

- 개발: `scripts/run-ka-hgis.ps1`
- 포터블: `dist/ka-hgis-portable/start.bat`

## 주의

- 포터블은 QGIS/Qt/GDAL DLL을 폴더에 넣습니다. git에는 올리지 않습니다.
- VWorld API 키는 대상 PC 로컬 설정 — 폴더에 넣지 않음
- GPLv2+ (QGIS 라이브러리 링크)
