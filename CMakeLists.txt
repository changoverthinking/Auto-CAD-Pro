cmake_minimum_required(VERSION 3.24)
project(AutoCADPro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ACP_BUILD_TESTS "Build Auto CAD Pro tests" ON)

add_library(acp_core
    src/core/geometry2d.cpp
    src/core/snap.cpp
    src/core/document.cpp
    src/core/history.cpp
    src/core/transform.cpp
    src/core/selection.cpp
    src/core/edit2d.cpp
)
target_include_directories(acp_core PUBLIC include)
target_compile_features(acp_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(acp_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(acp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ACP_BUILD_TESTS)
    enable_testing()
    add_executable(acp_core_tests tests/core_tests.cpp)
    target_link_libraries(acp_core_tests PRIVATE acp_core)
    add_test(NAME acp_core_tests COMMAND acp_core_tests)
endif()
