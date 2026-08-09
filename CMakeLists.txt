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
  src/core/ChecklistEngine.h
  src/core/SurveyProjectFactory.h
  src/core/ExportService.h
  src/core/ProjectStateBuilder.h
  src/core/LayoutService.h
  src/core/LayerOps.h
  src/core/LocationSearch.h
  src/core/WorkflowGuide.h
  src/core/VworldSettings.h
)
target_include_directories(ka_core PUBLIC ${KA_INCLUDES})
target_link_directories(ka_core PUBLIC ${KA_LIBDIRS})
target_link_libraries(ka_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui)
target_compile_definitions(ka_core PUBLIC KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

add_executable(ka-hgis
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/KaApplication.cpp
  src/app/KaIcons.cpp
  src/app/KaCaptureMapTool.cpp
  src/app/KaLayoutWindow.cpp
  src/app/MainWindow.h
  src/app/KaApplication.h
  src/app/KaIcons.h
  src/app/KaCaptureMapTool.h
  src/app/KaLayoutWindow.h
)
target_include_directories(ka-hgis PRIVATE ${KA_INCLUDES})
target_link_directories(ka-hgis PRIVATE ${KA_LIBDIRS})
target_link_libraries(ka-hgis PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui)
target_compile_definitions(ka-hgis PRIVATE KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

add_custom_command(TARGET ka-hgis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/rules"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/rules/drawing_checklist.v1.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/rules/drawing_checklist.v1.json"
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
endif()
