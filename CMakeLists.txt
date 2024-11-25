cmake_minimum_required(VERSION 3.0)
project(TPE-Protos C)

message("=== CMake ===")

set(CMAKE_BUILD_TYPE Debug)
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 11)

# Shared variables
set(PROJECT_ROOT_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "Project Source directory")
include_directories(${PROJECT_ROOT_DIR}/src/include)
message(STATUS "PROJECT_ROOT_DIR: ${PROJECT_ROOT_DIR}")

add_subdirectory(src)

# Set the compiler flags
set(CMAKE_C_FLAGS    "-pthread -g  -ggdb --std=c11 -pedantic -pedantic-errors -gdwarf-4 -fsanitize=address
                      -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L
                      -I ${INCLUDE_DIR}")

set(CLANG_TIDY_CHECKS *
        -boost-*
        -mpi-*
        -llvm-*
        -cppcoreguidelines-*
        -clang-analyzer-osx.*
        -cert-*-cpp
        -misc-unused-parameters,
        -clang-diagnostic-missing-declarations)
string(REPLACE ";" "," CLANG_TIDY_CHECKS "${CLANG_TIDY_CHECKS}")

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    find_program(CLANGTIDY NAMES clang-tidy)
    if(CLANGTIDY)
        message(STATUS "Found clang-tidy: ${CLANGTIDY}")
        set(CMAKE_C_CLANG_TIDY clang-tidy
                -checks=${CLANG_TIDY_CHECKS}
                -warnings-as-errors=*
                -fix)
    endif()
endif()
