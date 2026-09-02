cmake_minimum_required(VERSION 3.22)
project(lcdriv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(lcdriv INTERFACE)
add_library(lcdriv::lcdriv ALIAS lcdriv)
target_include_directories(lcdriv INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(lcdriv INTERFACE cxx_std_17)

option(LCDRIV_BUILD_TESTS "build unit tests" ON)
if(LCDRIV_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
    FetchContent_MakeAvailable(googletest)
    # 聚合目标：一次构建全部 UT（各 suite 由 lcdriv_add_ut 自动挂接）
    add_custom_target(lcdriv_ut
        COMMENT "Build all lcdriv unit tests"
    )
    add_subdirectory(tests)
endif()
