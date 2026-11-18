cmake_minimum_required(VERSION 3.10..3.30)

project(QOI_CPP LANGUAGES CXX C VERSION 0.0.1)

option(CHECK "Checks things like compiler warnings, mostly for devs" OFF)

add_library(${PROJECT_NAME} INTERFACE qoi.hpp)
target_include_directories(${PROJECT_NAME} INTERFACE .)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
install(TARGETS ${PROJECT_NAME})

function(enable_checks target_name)
    if (CHECK)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${target_name} PRIVATE -Weverything
                    -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat -Wno-padded
                    # Cannot do much about in a header only lib
                    -Wno-weak-vtables -Wno-unused-macros
            )
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target_name} PRIVATE /W4)
        else ()
            target_compile_options(${target_name} PRIVATE -Wall -Wextra)
        endif ()
    endif ()
endfunction()

if (BUILD_TESTS)
    add_executable(constexpr_test
            tests/constexpr_tests.cpp
    )
    enable_checks(constexpr_test)
    set_target_properties(constexpr_test PROPERTIES CXX_STANDARD 23)
    target_link_libraries(constexpr_test PRIVATE qoi_cpp)
endif ()

option(ENABLE_FUZZING "Build the fuzz testing application for qoi" OFF)

if (ENABLE_FUZZING)
    add_executable(qoi_fuzz qoifuzz.cpp)
    enable_checks(qoi_fuzz)
    set_target_properties(qoi_fuzz PROPERTIES CXX_STANDARD 23)
    target_compile_options(qoi_fuzz PRIVATE -fsanitize=fuzzer)
    target_link_options(qoi_fuzz PRIVATE -fsanitize=fuzzer)
    target_compile_options(qoi_fuzz PRIVATE -fsanitize=undefined)
    target_link_options(qoi_fuzz PRIVATE -fsanitize=undefined)
    target_compile_options(qoi_fuzz PRIVATE -fsanitize=memory)
    target_link_options(qoi_fuzz PRIVATE -fsanitize=memory)
    install(TARGETS qoi_fuzz)
endif ()
