# Desktop icon is the user-visible verify

User standing order: **모든 수정은 바탕화면 아이콘을 눌러 확인한다.**

Desktop shortcut `고고학 전용 HGIS.lnk` → `A:\hgis\dist\ka-hgis-portable\start.bat` → `ka-hgis.exe`.

## After every product C++ / UI / QSS change

1. `cmake --build build --config Release --target ka-hgis` (or full Release).
2. `.\scripts\publish-desktop.ps1` — copies `build\Release\ka-hgis.exe` onto the portable the icon launches. Stops a running `ka-hgis` if the copy is locked.
3. Tell the user to **close the old window** and click **고고학 전용 HGIS**.
4. Do not claim 완료 from `build\Release` only. Tests-only is not the field check.

`launch.ps1` prefers `build\Release` — the **icon does not**. Always publish to portable.
