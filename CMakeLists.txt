cmake_minimum_required(VERSION 3.21)
project(ka-hgis VERSION 0.3.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if (MSVC)
  add_compile_options(/MP)
  # /Zc:__cplusplus : 없으면 CMAKE_CXX_STANDARD 20 을 줘도 __cplusplus 가 199711(C++98)로 보고되어
  #                   라이브러리들의 C++20 기능 검출이 조용히 실패한다. (이 머신에서 실측 확인)
  # /utf-8          : 소스/실행 문자셋 UTF-8 고정 (기존에 타깃마다 흩어져 있던 것을 여기서 일괄 적용)
  add_compile_options(/Zc:__cplusplus /utf-8 /permissive-)
  # 경고 수준 상향. 자체 코드는 따옴표 include, Qt/QGIS 는 꺾쇠 include 이므로
  # /external:anglebrackets 로 외부 헤더 경고만 차단하고 우리 코드에는 /W4 를 적용한다.
  add_compile_options(/W4 /external:anglebrackets /external:W0)
endif()

# LTO(링크 타임 최적화): 번역 단위 경계를 넘어 인라이닝한다. Release 에서만.
# check_ipo_supported() 는 build/CMakeFiles 에 임시 테스트 프로젝트를 남겨
# VS 제너레이터의 ALL_BUILD 를 깨뜨리므로(MSB3202) 쓰지 않는다.
# MSVC 2022 는 /GL + /LTCG 를 확실히 지원하므로 바로 켠다.
if(MSVC)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
  message(STATUS "LTO: MSVC c2.dll 내부 오류 회피를 위해 OFF")
else()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT KA_IPO_OK OUTPUT KA_IPO_MSG)
  if(KA_IPO_OK)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(STATUS "LTO 미지원: ${KA_IPO_MSG}")
  endif()
endif()

option(KA_HGIS_BUILD_TESTS "Build tests" ON)
if (EXISTS "C:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "C:/OSGeo4W")
elseif (EXISTS "D:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "D:/OSGeo4W")
elseif (EXISTS "A:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "A:/OSGeo4W")
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
  src/core/SurveyStorage.cpp
  src/core/ExportService.cpp
  src/core/ProjectStateBuilder.cpp
  src/core/LayoutService.cpp
  src/core/LayerOps.cpp
  src/core/LocationSearch.cpp
  src/core/AdminBoundaryService.cpp
  src/core/KoreaRegionCatalog.cpp
  src/core/WorkflowGuide.cpp
  src/core/VworldSettings.cpp
  src/core/GeorefService.cpp
  src/core/FeaturePresets.cpp
  src/core/BufferAnalysis.cpp
  src/core/RecentSurveys.cpp
  src/core/KaSafeQgis.cpp
  src/core/MeasureOps.cpp
  src/core/DemAnalyzer.cpp
  src/core/TrenchGridGenerator.cpp
  src/core/TilePackService.cpp
  src/core/SoilMapService.cpp
  src/core/PaleoLandformService.cpp
  src/core/GeologyMapService.cpp
  src/core/RiverMapService.cpp
  src/core/SectionLayoutService.cpp
  src/core/Terrain3dService.cpp
  src/core/Terrain3dLayoutService.cpp
  src/core/AndongPack.cpp
  src/core/ChecklistEngine.h
  src/core/SurveyProjectFactory.h
  src/core/SurveyStorage.h
  src/core/ExportService.h
  src/core/ProjectStateBuilder.h
  src/core/LayoutService.h
  src/core/LayerOps.h
  src/core/LocationSearch.h
  src/core/AdminBoundaryService.h
  src/core/KoreaRegionCatalog.h
  src/core/WorkflowGuide.h
  src/core/VworldSettings.h
  src/core/GeorefService.h
  src/core/FeaturePresets.h
  src/core/BufferAnalysis.h
  src/core/RecentSurveys.h
  src/core/KaSafeQgis.h
  src/core/MeasureOps.h
  src/core/DemAnalyzer.h
  src/core/TrenchGridGenerator.h
  src/core/TilePackService.h
  src/core/SoilMapService.h
  src/core/PaleoLandformService.h
  src/core/GeologyMapService.h
  src/core/RiverMapService.h
  src/core/SectionLayoutService.h
  src/core/Terrain3dService.h
  src/core/Terrain3dLayoutService.h
  src/core/AndongPack.h
)
target_include_directories(ka_core PUBLIC ${KA_INCLUDES})
target_link_directories(ka_core PUBLIC ${KA_LIBDIRS})
target_link_libraries(ka_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i)
if (MSVC)
  target_compile_options(ka_core PRIVATE /openmp /utf-8)
endif()
target_compile_definitions(ka_core PUBLIC KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss")
file(READ "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss" KA_QSS_CONTENT)
string(FIND "${KA_QSS_CONTENT}" "/* ---- View tabs" _split_pos)
if(_split_pos GREATER 0)
  string(SUBSTRING "${KA_QSS_CONTENT}" 0 ${_split_pos} KA_QSS_PART1)
  string(SUBSTRING "${KA_QSS_CONTENT}" ${_split_pos} -1 KA_QSS_PART2)
else()
  set(KA_QSS_PART1 "${KA_QSS_CONTENT}")
  set(KA_QSS_PART2 "")
endif()
configure_file(
  "${CMAKE_SOURCE_DIR}/src/app/ka-hgis.qss.inc.in"
  "${CMAKE_BINARY_DIR}/generated/ka-hgis.qss.inc"
  @ONLY
)

add_executable(ka-hgis
  src/app/app.rc
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/KaBeginnerRibbon.cpp
  src/app/KaApplication.cpp
  src/app/KaTheme.cpp
  src/app/KaIcons.cpp
  src/app/KaCaptureMapTool.cpp
  src/app/KaAttributeMapTool.cpp
  src/app/KaLayoutWindow.cpp
  src/app/KaDrawingStudio.cpp
  src/app/KaSectionDrawingStudio.cpp
  src/app/KaTerrain3dStudio.cpp
  src/app/KaTerrain3dLayoutStudio.cpp
  src/app/KaTerrain3dView.cpp
  src/app/KaAlignMapTool.cpp
  src/app/KaImageView.cpp
  src/app/KaStartPage.cpp
  src/app/KaCoordPointMapTool.cpp
  src/app/KaMeasureMapTool.cpp
  src/app/KaCanvasGridOverlay.cpp
  src/app/KaTrenchMoveTool.cpp
  src/app/KaVertexEditTool.cpp
  src/app/KaFeatureSelectTool.cpp
  src/app/KaFoundLocationMark.cpp
  src/app/KaTrenchDialog.cpp
  src/app/KaDemClassDialog.cpp
  src/app/KaSurveyAreaDialog.cpp
  src/app/KaStatusBar.cpp
  src/app/KaCrashGuard.cpp
  src/app/KaRegionLocator.cpp
  src/app/KaFileBrowserPanel.cpp
  src/app/KaLayerOpacityRail.cpp
  src/app/MainWindow.h
  src/app/KaSurveyAreaDialog.h
  src/app/KaFileBrowserPanel.h
  src/app/KaLayerOpacityRail.h
  src/app/KaBeginnerRibbon.h
  src/app/KaApplication.h
  src/app/KaTheme.h
  src/app/KaIcons.h
  src/app/KaCaptureMapTool.h
  src/app/KaAttributeMapTool.h
  src/app/KaLayoutWindow.h
  src/app/KaDrawingStudio.h
  src/app/KaSectionDrawingStudio.h
  src/app/KaTerrain3dStudio.h
  src/app/KaTerrain3dLayoutStudio.h
  src/app/KaTerrain3dView.h
  src/app/KaAlignMapTool.h
  src/app/KaImageView.h
  src/app/KaStartPage.h
  src/app/KaCoordPointMapTool.h
  src/app/KaMeasureMapTool.h
  src/app/KaCanvasGridOverlay.h
  src/app/KaTrenchMoveTool.h
  src/app/KaVertexEditTool.h
  src/app/KaFeatureSelectTool.h
  src/app/KaFoundLocationMark.h
  src/app/KaTrenchDialog.h
  src/app/KaDemClassDialog.h
  src/app/KaStatusBar.h
  src/app/KaRegionLocator.h
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
  target_compile_options(ka-hgis PRIVATE /utf-8 "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(ka-hgis PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

add_executable(andong-viewer WIN32
  src/andong/AndongMain.cpp
  src/andong/AndongWindow.cpp
  src/andong/AndongWindow.h
)
target_include_directories(andong-viewer PRIVATE ${KA_INCLUDES})
target_link_directories(andong-viewer PRIVATE ${KA_LIBDIRS})
target_link_libraries(andong-viewer PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml qgis_core qgis_gui)
target_compile_definitions(andong-viewer PRIVATE KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")
if(MSVC)
  target_compile_options(andong-viewer PRIVATE /utf-8 "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(andong-viewer PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()
add_custom_command(TARGET andong-viewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:andong-viewer>/data/andong"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:andong-viewer>/data/theme"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/andong/andong_city.geojson"
    "$<TARGET_FILE_DIR:andong-viewer>/data/andong/andong_city.geojson"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/andong.qss"
    "$<TARGET_FILE_DIR:andong-viewer>/data/theme/andong.qss"
)
if(EXISTS "${CMAKE_SOURCE_DIR}/data/andong/andong_mark.png")
  add_custom_command(TARGET andong-viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/data/andong/andong_mark.png"
      "$<TARGET_FILE_DIR:andong-viewer>/data/andong/andong_mark.png")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/data/andong/satellite.mbtiles")
  add_custom_command(TARGET andong-viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/data/andong/satellite.mbtiles"
      "$<TARGET_FILE_DIR:andong-viewer>/data/andong/satellite.mbtiles")
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
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/korea_dongs.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/korea_dongs.json"
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
  if (MSVC)
    target_compile_options(ka_workflow_tests PRIVATE /utf-8)
  endif()
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
  if (MSVC)
    target_compile_options(ka_dem_trench_tests PRIVATE /utf-8)
  endif()
  add_test(NAME dem_trench_engine COMMAND ka_dem_trench_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_section_layout_tests tests/test_section_layout.cpp)
  target_link_libraries(ka_section_layout_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_section_layout_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_section_layout_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_section_layout_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME section_layout_engine COMMAND ka_section_layout_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_terrain_3d_tests tests/test_terrain_3d.cpp)
  target_link_libraries(ka_terrain_3d_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_terrain_3d_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_terrain_3d_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_terrain_3d_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_terrain_3d_tests PRIVATE /utf-8)
  endif()
  add_test(NAME terrain_3d_engine COMMAND ka_terrain_3d_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_executable(ka_section_studio_tests
    tests/test_section_studio.cpp
    src/app/KaSectionDrawingStudio.cpp
  )
  target_link_libraries(ka_section_studio_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_section_studio_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_link_directories(ka_section_studio_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_section_studio_tests PRIVATE ${KA_TEST_DEFS})
  add_test(NAME section_studio_engine COMMAND ka_section_studio_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  # Console subsystem, otherwise QtTest PASS/FAIL lines never reach ctest.
  set_target_properties(ka_hgis_tests ka_workflow_tests ka_georef_tests ka_preset_tests
                        ka_buffer_tests ka_measure_tests ka_dem_trench_tests
                        ka_section_layout_tests ka_section_studio_tests
                        ka_terrain_3d_tests
                        PROPERTIES WIN32_EXECUTABLE FALSE)

  add_executable(ka_andong_tests tests/test_andong.cpp)
  set_target_properties(ka_andong_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_andong_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_andong_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_andong_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_andong_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_andong_tests PRIVATE /utf-8)
  endif()
  add_test(NAME andong_pack COMMAND ka_andong_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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

  # Exercise the actual window's save/open slots, including project signal callbacks.
  get_target_property(KA_SAVE_OPEN_SOURCES ka-hgis SOURCES)
  list(REMOVE_ITEM KA_SAVE_OPEN_SOURCES src/app/main.cpp src/app/KaApplication.cpp src/app/app.rc)
  add_executable(ka_save_open_tests tests/test_save_open.cpp ${KA_SAVE_OPEN_SOURCES})
  set_target_properties(ka_save_open_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_save_open_tests PRIVATE ka_core Qt6::Test Qt6::Svg)
  target_include_directories(ka_save_open_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  if(MSVC)
    target_compile_options(ka_save_open_tests PRIVATE /utf-8 /FS)
  endif()
  add_test(NAME save_open_window COMMAND ka_save_open_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(save_open_window PROPERTIES TIMEOUT 90)

  add_executable(ka_e2e_tests tests/test_e2e_opaque.cpp)
  set_target_properties(ka_e2e_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_e2e_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_e2e_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_e2e_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_e2e_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_e2e_tests PRIVATE /utf-8 /W4)
  endif()
  add_test(NAME e2e_opaque_suite COMMAND ka_e2e_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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
    set_tests_properties(checklist_engine workflow_engine georef_engine feature_presets buffer_ring measure_tape dem_trench_engine theme_qss recent_surveys section_layout_engine section_studio_engine terrain_3d_engine save_open_window andong_pack e2e_opaque_suite PROPERTIES
      ENVIRONMENT "PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
  endif()
endif()
