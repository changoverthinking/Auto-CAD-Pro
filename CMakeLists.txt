cmake_minimum_required(VERSION 3.24)
project(AutoCADPro VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Produce a self-contained Windows executable that does not depend on
    # a separately installed Visual C++ runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(ACP_BUILD_TESTS "Build Auto CAD Pro tests" ON)
option(ACP_BUILD_APP "Build Auto CAD Pro Windows desktop application" ON)
option(ACP_ENABLE_GUI_TEST_HOOKS "Enable test-only GUI integration hooks" OFF)

set(ACP_PDF_FONT_DIR "${CMAKE_BINARY_DIR}/assets")
set(ACP_PDF_FONT_FILE "${ACP_PDF_FONT_DIR}/NotoSansJP.ttf")
set(ACP_PDF_FONT_URL "https://raw.githubusercontent.com/google/fonts/e44c4b011a820c2cbe2fd2cfa8052037d7edb571/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf")
set(ACP_PDF_FONT_EXPECTED_SIZE 9589900)

if(ACP_BUILD_APP OR ACP_BUILD_TESTS)
    file(MAKE_DIRECTORY "${ACP_PDF_FONT_DIR}")
    if(NOT EXISTS "${ACP_PDF_FONT_FILE}")
        message(STATUS "Downloading pinned Noto Sans JP PDF font asset")
        file(DOWNLOAD
            "${ACP_PDF_FONT_URL}"
            "${ACP_PDF_FONT_FILE}"
            TLS_VERIFY ON
            STATUS ACP_PDF_FONT_DOWNLOAD_STATUS
            SHOW_PROGRESS)
        list(GET ACP_PDF_FONT_DOWNLOAD_STATUS 0 ACP_PDF_FONT_DOWNLOAD_CODE)
        list(GET ACP_PDF_FONT_DOWNLOAD_STATUS 1 ACP_PDF_FONT_DOWNLOAD_MESSAGE)
        if(NOT ACP_PDF_FONT_DOWNLOAD_CODE EQUAL 0)
            file(REMOVE "${ACP_PDF_FONT_FILE}")
            message(FATAL_ERROR "Could not download pinned PDF font: ${ACP_PDF_FONT_DOWNLOAD_MESSAGE}")
        endif()
    endif()

    file(SIZE "${ACP_PDF_FONT_FILE}" ACP_PDF_FONT_ACTUAL_SIZE)
    if(NOT ACP_PDF_FONT_ACTUAL_SIZE EQUAL ACP_PDF_FONT_EXPECTED_SIZE)
        file(REMOVE "${ACP_PDF_FONT_FILE}")
        message(FATAL_ERROR
            "Pinned PDF font size mismatch: expected ${ACP_PDF_FONT_EXPECTED_SIZE}, got ${ACP_PDF_FONT_ACTUAL_SIZE}")
    endif()
endif()

add_library(acp_core
    src/core/geometry2d.cpp
    src/core/geometry3d.cpp
    src/core/model3d.cpp
    src/core/architecture3d.cpp
    src/core/architecture3d_hosting.cpp
    src/core/architecture3d_advanced.cpp
    src/core/architecture3d_storeys.cpp
    src/core/architecture3d_roof.cpp
    src/core/architecture3d_builder.cpp
    src/core/obj.cpp
    src/core/blender_bridge.cpp
    src/core/viewport3d.cpp
    src/core/snap.cpp
    src/core/document.cpp
    src/core/history.cpp
    src/core/property_edit.cpp
    src/core/transform.cpp
    src/core/selection.cpp
    src/core/edit2d.cpp
    src/core/block.cpp
    src/core/annotation.cpp
    src/core/hatch.cpp
    src/core/persistence.cpp
    src/core/dxf.cpp
    src/core/bounds.cpp
    src/core/layout.cpp
    src/core/ui_layout.cpp
    src/core/svg.cpp
    src/core/pdf.cpp
    src/core/recovery.cpp
)
target_include_directories(acp_core PUBLIC include PRIVATE third_party/stb)
target_compile_features(acp_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(acp_core PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(acp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ACP_BUILD_APP AND WIN32)
    add_executable(AutoCADPro WIN32 src/app/main_win.cpp src/app/app.rc)
    target_link_libraries(AutoCADPro PRIVATE acp_core user32 gdi32 comdlg32 shell32)
    target_compile_definitions(AutoCADPro PRIVATE UNICODE _UNICODE)
    if(ACP_ENABLE_GUI_TEST_HOOKS)
        target_compile_definitions(AutoCADPro PRIVATE ACP_ENABLE_GUI_TEST_HOOKS=1)
    endif()
    target_compile_features(AutoCADPro PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(AutoCADPro PRIVATE /W4 /permissive- /EHsc /utf-8)
        target_link_options(AutoCADPro PRIVATE
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/app/app.manifest")
    endif()

    install(TARGETS AutoCADPro RUNTIME DESTINATION .)
    install(FILES README.md DESTINATION .)
    install(FILES "${ACP_PDF_FONT_FILE}" DESTINATION assets)
    install(FILES third_party/noto/OFL.txt third_party/noto/SOURCE.md DESTINATION licenses/NotoSansJP)
    install(FILES third_party/stb/LICENSE DESTINATION licenses/stb)
endif()

if(ACP_BUILD_TESTS)
    enable_testing()
    add_executable(acp_core_tests tests/core_tests.cpp)
    target_link_libraries(acp_core_tests PRIVATE acp_core)
    target_compile_definitions(acp_core_tests PRIVATE
        ACP_TEST_PDF_FONT_PATH="${ACP_PDF_FONT_FILE}")
    add_test(NAME acp_core_tests COMMAND acp_core_tests)

    add_executable(acp_workflow_regression_tests tests/workflow_regression_tests.cpp)
    target_link_libraries(acp_workflow_regression_tests PRIVATE acp_core)
    add_test(NAME acp_workflow_regression_tests COMMAND acp_workflow_regression_tests)

    add_executable(acp_edit_tools_workflow_tests tests/edit_tools_workflow_tests.cpp)
    target_link_libraries(acp_edit_tools_workflow_tests PRIVATE acp_core)
    add_test(NAME acp_edit_tools_workflow_tests COMMAND acp_edit_tools_workflow_tests)

    add_executable(acp_annotation_properties_workflow_tests tests/annotation_properties_workflow_tests.cpp)
    target_link_libraries(acp_annotation_properties_workflow_tests PRIVATE acp_core)
    add_test(NAME acp_annotation_properties_workflow_tests COMMAND acp_annotation_properties_workflow_tests)

    add_executable(acp_property_edit_tests tests/property_edit_tests.cpp)
    target_link_libraries(acp_property_edit_tests PRIVATE acp_core)
    add_test(NAME acp_property_edit_tests COMMAND acp_property_edit_tests)

    add_executable(acp_color_linetype_tests tests/color_linetype_tests.cpp)
    target_link_libraries(acp_color_linetype_tests PRIVATE acp_core)
    add_test(NAME acp_color_linetype_tests COMMAND acp_color_linetype_tests)

    add_executable(acp_recovery_tests tests/recovery_tests.cpp)
    target_link_libraries(acp_recovery_tests PRIVATE acp_core)
    add_test(NAME acp_recovery_tests COMMAND acp_recovery_tests)

    add_executable(acp_layout_print_workflow_tests tests/layout_print_workflow_tests.cpp)
    target_link_libraries(acp_layout_print_workflow_tests PRIVATE acp_core)
    target_compile_definitions(acp_layout_print_workflow_tests PRIVATE
        ACP_TEST_PDF_FONT_PATH="${ACP_PDF_FONT_FILE}")
    add_test(NAME acp_layout_print_workflow_tests COMMAND acp_layout_print_workflow_tests)

    add_executable(acp_large_document_tests tests/large_document_tests.cpp)
    target_link_libraries(acp_large_document_tests PRIVATE acp_core)
    add_test(NAME acp_large_document_tests COMMAND acp_large_document_tests)

    add_executable(acp_ui_layout_tests tests/ui_layout_tests.cpp)
    target_link_libraries(acp_ui_layout_tests PRIVATE acp_core)
    add_test(NAME acp_ui_layout_tests COMMAND acp_ui_layout_tests)

    add_executable(acp_model3d_blender_tests tests/model3d_blender_tests.cpp)
    target_link_libraries(acp_model3d_blender_tests PRIVATE acp_core)
    add_test(NAME acp_model3d_blender_tests COMMAND acp_model3d_blender_tests)

    add_executable(acp_architecture3d_tests tests/architecture3d_tests.cpp)
    target_link_libraries(acp_architecture3d_tests PRIVATE acp_core)
    add_test(NAME acp_architecture3d_tests COMMAND acp_architecture3d_tests)

    add_executable(acp_opening_element_tests tests/opening_element_tests.cpp)
    target_link_libraries(acp_opening_element_tests PRIVATE acp_core)
    add_test(NAME acp_opening_element_tests COMMAND acp_opening_element_tests)

    add_executable(acp_architecture3d_advanced_tests tests/architecture3d_advanced_tests.cpp)
    target_link_libraries(acp_architecture3d_advanced_tests PRIVATE acp_core)
    add_test(NAME acp_architecture3d_advanced_tests COMMAND acp_architecture3d_advanced_tests)

    add_executable(acp_document_revision_tests tests/document_revision_tests.cpp)
    target_link_libraries(acp_document_revision_tests PRIVATE acp_core)
    add_test(NAME acp_document_revision_tests COMMAND acp_document_revision_tests)

    add_executable(acp_architecture3d_storeys_tests tests/architecture3d_storeys_tests.cpp)
    target_link_libraries(acp_architecture3d_storeys_tests PRIVATE acp_core)
    add_test(NAME acp_architecture3d_storeys_tests COMMAND acp_architecture3d_storeys_tests)

    add_executable(acp_architecture3d_roof_cache_tests tests/architecture3d_roof_cache_tests.cpp)
    target_link_libraries(acp_architecture3d_roof_cache_tests PRIVATE acp_core)
    add_test(NAME acp_architecture3d_roof_cache_tests COMMAND acp_architecture3d_roof_cache_tests)

    add_executable(acp_architecture3d_builder_tests tests/architecture3d_builder_tests.cpp)
    target_link_libraries(acp_architecture3d_builder_tests PRIVATE acp_core)
    add_test(NAME acp_architecture3d_builder_tests COMMAND acp_architecture3d_builder_tests)
endif()

if(WIN32 AND ACP_BUILD_APP)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "AutoCADPro")
    set(CPACK_PACKAGE_VENDOR "Auto CAD Pro")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "AutoCADPro-${PROJECT_VERSION}-Windows-x64")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
endif()
