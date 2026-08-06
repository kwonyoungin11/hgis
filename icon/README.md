# ka-hgis icons

App icon candidates for **ka-hgis** (field archaeology HGIS).

| File | Use |
|------|-----|
| `ka-hgis-icon.svg` | Vector source |
| `ka-hgis-icon.png` | Master 512×512 |
| `ka-hgis-16.png` … `ka-hgis-512.png` | Fixed sizes |
| `ka-hgis.ico` | Windows multi-size icon |

## Design

- Teal → earth gradient tile (GIS + soil)
- Map sheet + excavation polygon
- Gold control-point / GPS crosshair
- Not wired into the app yet (assets only)

## Regenerate PNGs

```powershell
python icon\render_icons.py
```
