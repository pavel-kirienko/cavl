# This software is distributed under the terms of the MIT License.
# Copyright (c) 2021 Pavel Kirienko.
# Author: Pavel Kirienko <pavel@uavcan.org>

cmake_minimum_required(VERSION 3.12)
project(cavl_tests C CXX)
enable_testing()

set(CTEST_OUTPUT_ON_FAILURE ON)

# Use -DNO_STATIC_ANALYSIS=1 to suppress static analysis. If not suppressed, the tools used here shall be available.
if (NOT NO_STATIC_ANALYSIS)
    find_program(clang_tidy NAMES clang-tidy-12 clang-tidy)
    if (NOT clang_tidy)
        message(FATAL_ERROR "Could not locate clang-tidy")
    endif ()
    message(STATUS "Using clang-tidy: ${clang_tidy}")
    set(CMAKE_CXX_CLANG_TIDY ${clang_tidy})
endif ()

find_program(clang_format NAMES clang-format-12 clang-format)
if (NOT clang_format)
    message(STATUS "Could not locate clang-format")
else ()
    file(GLOB format_files ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch] ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]pp)
    message(STATUS "Using clang-format: ${clang_format}; files: ${format_files}")
    add_custom_target(format COMMAND ${clang_format} -i -fallback-style=none -style=file --verbose ${format_files})
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic -fstrict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wsign-promo")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wcast-align -Wmissing-declarations")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wtype-limits -Wnon-virtual-dtor -Woverloaded-virtual")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")

add_library(unity STATIC ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity.c)
target_include_directories(unity SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)
target_compile_definitions(unity PUBLIC -DUNITY_SHORTHAND_AS_RAW=1 -DUNITY_OUTPUT_COLOR=1)

add_executable(test_c ${CMAKE_CURRENT_SOURCE_DIR}/c/test.cpp)
target_link_libraries(test_c unity)
add_test("run_test_c" "test_c")

add_executable(test_cpp ${CMAKE_CURRENT_SOURCE_DIR}/c++/test.cpp)
target_link_libraries(test_cpp unity)
add_test("run_test_cpp" "test_cpp")
