function(lcdriv_add_ut TEST_NAME)
    cmake_parse_arguments(UT "" "" "SOURCES" ${ARGN})

    add_executable(${TEST_NAME} ${UT_SOURCES})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main lcdriv)

    add_dependencies(lcdriv_ut ${TEST_NAME})

    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME} PROPERTIES LABELS "lcdriv")
endfunction()

lcdriv_add_ut(TestCompileTime SOURCES TestCompileTime.cpp)

add_executable(lcdriv_restricted RestrictedCompile.cpp)
target_include_directories(lcdriv_restricted PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(lcdriv_restricted PRIVATE lcdriv)
target_compile_options(lcdriv_restricted PRIVATE
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -Wall
    -Wextra
    -Werror
)
add_dependencies(lcdriv_ut lcdriv_restricted)
