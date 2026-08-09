---
name: context7
description: 7-layer context analysis skill for deep codebase understanding, history tracing, and constraint verification.
---

# Context7 Skill (7계층 컨텍스트 분석)

Use this skill when analyzing large codebases, complex refactoring tasks, or multi-component applications.

## The 7 Layers of Context

1. **Codebase Architecture**: Structure, component boundaries, data flow.
2. **Commit History & Transcripts**: Historical context, design rationale, past decisions.
3. **Dependency Tree**: Frameworks, SDKs, external C++/Qt6/QGIS/GDAL/Python libraries.
4. **Data Schemas & Contracts**: GPKG schemas, JSON rule formats, WMS/WMTS endpoints.
5. **Runtime Environment**: OS, PATH, environment variables (`OSGEO4W_ROOT`, `QGIS_PREFIX_PATH`).
6. **User Intent & Constraints**: Direct user directives, UI/UX requirements, explicit rules.
7. **Verification & Audit**: Automated test suites, smoke tests, logs, and empirical validation.
