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
set_target_properties(constexpr_test PROPERTIES CXX_STANDARD 20)
target_link_libraries(constexpr_test PRIVATE qoi_cpp)

add_library(qoi_c INTERFACE qoi.h)

option(build_conv "Build the qoi <> png converter" OFF)
if (build_conv)
    add_executable(qoi_conv qoiconv.c)
    set_target_properties(qoi_conv PROPERTIES C_STANDARD 11)
    target_link_libraries(qoi_conv PRIVATE qoi_c)
endif()

option(build_qoi_fuzz "Build the fuzz testing application for qoi" OFF)

if (build_qoi_fuzz)
    add_executable(qoi_fuzz qoifuzz.c)
    target_compile_options(qoi_fuzz PRIVATE
            "-fsanitize=address"
            -lasan
    )
endif()
