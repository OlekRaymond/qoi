cmake_minimum_required(VERSION 3.10..3.30)

project(QOI_CPP LANGUAGES CXX C VERSION 0.0.1)

# TODO: Turn off by default on first release
option(CHECK "Checks things like compiler warnings, mostly for devs" ON)

add_library(qoi_cpp INTERFACE qoi.hpp)
target_include_directories(qoi_cpp INTERFACE .)
set_target_properties(qoi_cpp PROPERTIES CXX_STANDARD 20)
add_executable(constexpr_test
        tests/constexpr_tests.cpp
)
if (CHECK)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(constexpr_test PRIVATE -Weverything
            -Wno-c++98-compat
        )
    else()
        target_compile_options(constexpr_test PRIVATE -Wall -Wextra)
    endif ()
endif()
set_target_properties(constexpr_test PROPERTIES CXX_STANDARD 23)
target_link_libraries(constexpr_test PRIVATE qoi_cpp)

option(ENABLE_FUZZING "Build the fuzz testing application for qoi" OFF)

if (ENABLE_FUZZING)
    add_executable(qoi_fuzz qoifuzz.cpp)
    target_compile_options(qoi_fuzz PRIVATE
        "-fsanitize=address"
        -lasan
    )
endif()
