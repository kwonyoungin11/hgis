# ka-hgis 도메인 데이터 모델

기본 좌표계: **EPSG:5179** (Korea 2000 / Unified CS, UTM-K GRS80)

작업 저장: **GeoPackage** (`survey.gpkg`)  
제출: SHP/PDF (내보내기 전용)

## 레이어

### survey_area (조사구역) — Polygon
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| survey_name | string | Y | 조사명 |
| site_name | string | N | 유적명 |
| note | string | N | 비고 |

**제약:** 실제 폴리곤만. 점/원 등 추상 마커 금지.

### feature_poly (유구 면) — Polygon
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| kind | string | Y | 유구종류 |
| period | string | Y | 시대 |
| feature_no | string | N | 유구번호 |
| note | string | N | 비고 |

### feature_line (유구/경계 선) — LineString
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| kind | string | Y | 종류(단면선, 경계 등) |
| period | string | N | 시대 |
| note | string | N | 비고 |

### section_line (층위·단면 기준선) — LineString
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| section_id | string | Y | 단면 번호 |
| note | string | N | 비고 |

### control_points (GPS 기준점) — Point
| 필드 | 형 | 필수 | 설명 |
| --- | --- | --- | --- |
| point_id | string | Y | 점 이름 |
| x | double | Y | X (또는 경도) |
| y | double | Y | Y (또는 위도) |
| z | double | N | 표고 |
| datum | string | Y | 측지기준계 |
| ellipsoid | string | Y | 타원체 |
| projection | string | Y | 투영 |
| origin | string | N | 투영원점 |
| accuracy | string | N | 정확도 메모 (2DRMS 등) |

**제약:** 제출 검수 시 **≥ 2점**.

## YAML 스키마 원본 (배포 시 `data/schemas/ka_hgis_layers.yaml`)

```yaml
version: 1
default_crs: EPSG:5179
working_format: gpkg
layers:
  - id: survey_area
    geometry: Polygon
    name_ko: 조사구역
    fields:
      - {name: survey_name, type: string, required: true}
      - {name: site_name, type: string, required: false}
      - {name: note, type: string, required: false}
    constraints: [real_polygon_only, no_abstract_marker]
  - id: feature_poly
    geometry: Polygon
    name_ko: 유구_면
    fields:
      - {name: kind, type: string, required: true}
      - {name: period, type: string, required: true}
      - {name: feature_no, type: string, required: false}
      - {name: note, type: string, required: false}
  - id: feature_line
    geometry: LineString
    name_ko: 유구_선
    fields:
      - {name: kind, type: string, required: true}
      - {name: period, type: string, required: false}
      - {name: note, type: string, required: false}
  - id: section_line
    geometry: LineString
    name_ko: 단면선
    fields:
      - {name: section_id, type: string, required: true}
      - {name: note, type: string, required: false}
  - id: control_points
    geometry: Point
    name_ko: 기준점
    fields:
      - {name: point_id, type: string, required: true}
      - {name: x, type: double, required: true}
      - {name: y, type: double, required: true}
      - {name: z, type: double, required: false}
      - {name: datum, type: string, required: true}
      - {name: ellipsoid, type: string, required: true}
      - {name: projection, type: string, required: true}
      - {name: origin, type: string, required: false}
      - {name: accuracy, type: string, required: false}
    constraints: [min_count_for_submit: 2]
```
