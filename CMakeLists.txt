cmake_minimum_required(VERSION 3.24)
project(AutoCADPro VERSION 0.1.0 LANGUAGES CXX)

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

add_library(acp_core
    src/core/geometry2d.cpp
    src/core/snap.cpp
    src/core/document.cpp
    src/core/history.cpp
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
    src/core/svg.cpp
    src/core/pdf.cpp
    src/core/recovery.cpp
)
target_include_directories(acp_core PUBLIC include)
target_compile_features(acp_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(acp_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(acp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ACP_BUILD_APP AND WIN32)
    add_executable(AutoCADPro WIN32 src/app/main_win.cpp)
    target_link_libraries(AutoCADPro PRIVATE acp_core user32 gdi32 comdlg32)
    target_compile_definitions(AutoCADPro PRIVATE UNICODE _UNICODE)
    if(ACP_ENABLE_GUI_TEST_HOOKS)
        target_compile_definitions(AutoCADPro PRIVATE ACP_ENABLE_GUI_TEST_HOOKS=1)
    endif()
    target_compile_features(AutoCADPro PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(AutoCADPro PRIVATE /W4 /permissive- /EHsc)
    endif()
endif()

if(ACP_BUILD_TESTS)
    enable_testing()
    add_executable(acp_core_tests tests/core_tests.cpp)
    target_link_libraries(acp_core_tests PRIVATE acp_core)
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

    add_executable(acp_recovery_tests tests/recovery_tests.cpp)
    target_link_libraries(acp_recovery_tests PRIVATE acp_core)
    add_test(NAME acp_recovery_tests COMMAND acp_recovery_tests)

    add_executable(acp_layout_print_workflow_tests tests/layout_print_workflow_tests.cpp)
    target_link_libraries(acp_layout_print_workflow_tests PRIVATE acp_core)
    add_test(NAME acp_layout_print_workflow_tests COMMAND acp_layout_print_workflow_tests)

    add_executable(acp_large_document_tests tests/large_document_tests.cpp)
    target_link_libraries(acp_large_document_tests PRIVATE acp_core)
    add_test(NAME acp_large_document_tests COMMAND acp_large_document_tests)
endif()
