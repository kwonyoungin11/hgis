# Official QGIS + ArcGIS developer sites first (user-forced)

On every develop / fix / GIS request:

1. Follow the **user's last command**.
2. Learn the matching topic in **English** from official developer sites before editing:
   - QGIS PyQGIS cookbook: https://docs.qgis.org/latest/en/docs/pyqgis_developer_cookbook/
   - QGIS User Manual: https://docs.qgis.org/latest/en/docs/user_manual/
   - QGIS C++ API: https://api.qgis.org/api/
   - In-repo: `docs/vendor/qgis-manual-3.44/`
   - ArcGIS developers: https://developers.arcgis.com/documentation/
   - ArcGIS Pro help (graphics/map): https://pro.arcgis.com/en/pro-app/latest/help/mapping/
   - Terms only: `docs/user/job-cards/arcgis-용어.md` — do not clone ArcMap chrome
3. Use context7 + `open_page` / `web_fetch` on those URLs. Do not invent `Qgs*` APIs.
4. Hooks inject the same text (`excavation-loop.txt` via UserPromptSubmit). This rule is the durable copy.