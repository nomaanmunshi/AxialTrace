cmake_minimum_required(VERSION 3.16)
project(AxialTrace LANGUAGES CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(axis src/axis.cpp)
target_include_directories(axis PUBLIC src)
add_library(replay src/replay.cpp)
target_include_directories(replay PUBLIC src)
add_executable(axialtrace src/main.cpp)
target_link_libraries(axialtrace PRIVATE axis replay)

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.15.2
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)
    FetchContent_MakeAvailable(googletest)
    add_executable(axis_tests tests/axis_test.cpp tests/replay_test.cpp)
    target_compile_definitions(axis_tests PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(axis_tests PRIVATE axis replay gtest_main)
    include(GoogleTest)
    gtest_discover_tests(axis_tests)
    add_test(NAME Cli.Contract
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:axialtrace>
            -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake)
endif()
