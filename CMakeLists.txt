cmake_minimum_required(VERSION 3.21)
project(ka-hgis VERSION 0.3.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(KA_HGIS_BUILD_TESTS "Build tests" ON)
if (EXISTS "C:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "C:/OSGeo4W")
else()
  set(OSGEO4W_ROOT_DEFAULT "D:/OSGeo4W")
endif()
set(OSGEO4W_ROOT "${OSGEO4W_ROOT_DEFAULT}" CACHE PATH "OSGeo4W root")
set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev" CACHE PATH "QGIS prefix")
list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml PrintSupport Network Svg Test)

set(KA_INCLUDES
  ${CMAKE_SOURCE_DIR}/src
  ${QGIS_PREFIX_PATH}/include
  ${OSGEO4W_ROOT}/apps/gdal-dev/include
  ${OSGEO4W_ROOT}/include
)
set(KA_LIBDIRS
  ${QGIS_PREFIX_PATH}/lib
  ${OSGEO4W_ROOT}/apps/gdal-dev/lib
  ${OSGEO4W_ROOT}/lib
)

add_library(ka_core STATIC
  src/core/ChecklistEngine.cpp
  src/core/SurveyProjectFactory.cpp
  src/core/ExportService.cpp
  src/core/ProjectStateBuilder.cpp
  src/core/LayoutService.cpp
  src/core/LayerOps.cpp
  src/core/LocationSearch.cpp
  src/core/WorkflowGuide.cpp
  src/core/VworldSettings.cpp
  src/core/GeorefService.cpp
  src/core/FeaturePresets.cpp
  src/core/BufferAnalysis.cpp
  src/core/RecentSurveys.cpp
  src/core/MeasureOps.cpp
  src/core/DemAnalyzer.cpp
  src/core/TrenchGridGenerator.cpp
  src/core/SoilMapService.cpp
  src/core/GeologyMapService.cpp
  src/core/RiverMapService.cpp
  src/core/ChecklistEngine.h
  src/core/SurveyProjectFactory.h
  src/core/ExportService.h
  src/core/ProjectStateBuilder.h
  src/core/LayoutService.h
  src/core/LayerOps.h
  src/core/LocationSearch.h
  src/core/WorkflowGuide.h
  src/core/VworldSettings.h
  src/core/GeorefService.h
  src/core/FeaturePresets.h
  src/core/BufferAnalysis.h
  src/core/RecentSurveys.h
  src/core/MeasureOps.h
  src/core/DemAnalyzer.h
  src/core/TrenchGridGenerator.h
  src/core/SoilMapService.h
  src/core/GeologyMapService.h
  src/core/RiverMapService.h
)
target_include_directories(ka_core PUBLIC ${KA_INCLUDES})
target_link_directories(ka_core PUBLIC ${KA_LIBDIRS})
target_link_libraries(ka_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i)
if (MSVC)
  target_compile_options(ka_core PRIVATE /openmp)
endif()
target_compile_definitions(ka_core PUBLIC KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss")
file(READ "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss" KA_QSS_CONTENT)
configure_file(
  "${CMAKE_SOURCE_DIR}/src/app/ka-hgis.qss.inc.in"
  "${CMAKE_BINARY_DIR}/generated/ka-hgis.qss.inc"
  @ONLY
)

add_executable(ka-hgis
  src/app/app.rc
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/KaApplication.cpp
  src/app/KaTheme.cpp
  src/app/KaIcons.cpp
  src/app/KaCaptureMapTool.cpp
  src/app/KaAttributeMapTool.cpp
  src/app/KaLayoutWindow.cpp
  src/app/KaDrawingStudio.cpp
  src/app/KaAlignMapTool.cpp
  src/app/KaImageView.cpp
  src/app/KaStartPage.cpp
  src/app/KaCoordPointMapTool.cpp
  src/app/KaMeasureMapTool.cpp
  src/app/KaCanvasGridOverlay.cpp
  src/app/KaTrenchMoveTool.cpp
  src/app/KaTrenchDialog.cpp
  src/app/KaStatusBar.cpp
  src/app/KaCrashGuard.cpp
  src/app/MainWindow.h
  src/app/KaApplication.h
  src/app/KaTheme.h
  src/app/KaIcons.h
  src/app/KaCaptureMapTool.h
  src/app/KaAttributeMapTool.h
  src/app/KaLayoutWindow.h
  src/app/KaDrawingStudio.h
  src/app/KaAlignMapTool.h
  src/app/KaImageView.h
  src/app/KaStartPage.h
  src/app/KaCoordPointMapTool.h
  src/app/KaMeasureMapTool.h
  src/app/KaCanvasGridOverlay.h
  src/app/KaTrenchMoveTool.h
  src/app/KaTrenchDialog.h
  src/app/KaStatusBar.h
  src/app/KaCrashGuard.h
)
target_include_directories(ka-hgis PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
target_link_directories(ka-hgis PRIVATE ${KA_LIBDIRS})
target_link_libraries(ka-hgis PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui)
target_compile_definitions(ka-hgis PRIVATE KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

# 릴리스에도 PDB를 만들어 크래시 로그(KaCrashGuard)가 함수명·줄번호까지 심볼화하게 한다.
# /FS: 병렬 컴파일에서 같은 PDB에 안전하게 쓰도록(C1041 방지).
if(MSVC)
  target_compile_options(ka_core PRIVATE "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_compile_options(ka-hgis PRIVATE "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(ka-hgis PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

add_custom_command(TARGET ka-hgis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/rules"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/rules/drawing_checklist.v1.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/rules/drawing_checklist.v1.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/styles"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/styles/feature_presets.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/styles/feature_presets.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/theme"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.qss"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.ico"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.ico"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/ka-hgis-실행.bat"
    "$<TARGET_FILE_DIR:ka-hgis>/ka-hgis-실행.bat"
)

if(KA_HGIS_BUILD_TESTS)
  enable_testing()
  set(KA_TEST_DEFS KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")
  set(KA_TEST_LIBS ka_core Qt6::Test Qt6::Core Qt6::Xml Qt6::Gui Qt6::Widgets Qt6::PrintSupport qgis_core qgis_gui)

  add_executable(ka_hgis_tests tests/test_checklist.cpp)
  target_link_libraries(ka_hgis_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_hgis_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_hgis_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_hgis_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME checklist_engine COMMAND ka_hgis_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_workflow_tests tests/test_workflow.cpp)
  target_link_libraries(ka_workflow_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_workflow_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_workflow_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_workflow_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME workflow_engine COMMAND ka_workflow_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_georef_tests tests/test_georef.cpp)
  target_link_libraries(ka_georef_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_georef_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_georef_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_georef_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME georef_engine COMMAND ka_georef_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_preset_tests tests/test_presets.cpp)
  target_link_libraries(ka_preset_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_preset_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_preset_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_preset_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME feature_presets COMMAND ka_preset_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_buffer_tests tests/test_buffer.cpp)
  target_link_libraries(ka_buffer_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_buffer_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_buffer_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_buffer_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME buffer_ring COMMAND ka_buffer_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_measure_tests tests/test_measure.cpp)
  target_link_libraries(ka_measure_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_measure_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_measure_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_measure_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME measure_tape COMMAND ka_measure_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_dem_trench_tests tests/test_dem_trench.cpp)
  target_link_libraries(ka_dem_trench_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_dem_trench_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_dem_trench_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_dem_trench_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME dem_trench_engine COMMAND ka_dem_trench_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  # Console subsystem, otherwise QtTest PASS/FAIL lines never reach ctest.
  set_target_properties(ka_hgis_tests ka_workflow_tests ka_georef_tests ka_preset_tests
                        ka_buffer_tests ka_measure_tests ka_dem_trench_tests
                        PROPERTIES WIN32_EXECUTABLE FALSE)

  add_executable(ka_theme_tests
    tests/test_theme.cpp
    src/app/KaTheme.cpp
  )
  set_target_properties(ka_theme_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_theme_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_theme_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  )
  add_test(NAME theme_qss COMMAND ka_theme_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_recent_tests
    tests/test_recent.cpp
    src/core/RecentSurveys.cpp
  )
  set_target_properties(ka_recent_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_recent_tests PRIVATE Qt6::Test Qt6::Core)
  target_include_directories(ka_recent_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  add_test(NAME recent_surveys COMMAND ka_recent_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  # Windows: ctest without OSGeo4W PATH → 0xc0000135 (DLL not found).
  # ENVIRONMENT is a CMake list, so unescaped ';' would truncate PATH to the
  # first bin dir. Escape path separators; keep ';' only between VAR=value.
  if(WIN32 AND EXISTS "${OSGEO4W_ROOT}/apps/qgis-dev/bin")
    set(_ka_path
      "${OSGEO4W_ROOT}/apps/qgis-dev/bin"
      "${OSGEO4W_ROOT}/apps/Qt6/bin"
      "${OSGEO4W_ROOT}/apps/gdal-dev/bin"
      "${OSGEO4W_ROOT}/apps/pdal-dev/bin"
      "${OSGEO4W_ROOT}/bin"
    )
    string(JOIN "\\;" _ka_path_esc ${_ka_path})
    set_tests_properties(checklist_engine workflow_engine georef_engine feature_presets buffer_ring measure_tape dem_trench_engine theme_qss recent_surveys PROPERTIES
      ENVIRONMENT "PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
  endif()
endif()
