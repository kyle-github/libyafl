# CMake build configuration for fcontext
# Derived from Boost.Context - https://github.com/boostorg/context
# Copyright Oliver Kowalke (original), Kyle Hayes (2026, CMake changes)
# Distributed under the Boost Software License 1.0
# See LICENSE file for details

cmake_minimum_required(VERSION 3.10)
project(fcontext_tests C)

# ============================================================================
# Enforce Out-of-Source Builds
# ============================================================================

# Enforce out-of-source builds to keep source tree clean
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please remove CMakeCache.txt and CMakeFiles/ from the source directory, then create a separate build directory: mkdir build && cd build && cmake ..")
endif()

# ============================================================================
# Output Directory Configuration
# ============================================================================

# Set all build products to go into the build directory
# This ensures binaries, libraries, and object files stay out of source tree
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For multi-config generators (Visual Studio, Xcode), ensure outputs are in build/bin not build/Debug/bin
# This works for both single-config (Unix Makefiles) and multi-config generators
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/lib)

# No debug suffix - keeps names clean
set(CMAKE_DEBUG_POSTFIX "")

# ============================================================================
# Language Configuration
# ============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_COVERAGE "Enable code coverage generation" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage")
        add_compile_options(--coverage)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

# Note: Assembly language will be enabled after architecture detection
# This allows us to set the correct compiler before enabling the language

# ============================================================================
# Compiler Flags
# ============================================================================

# Enable ASAN/UBSAN in Debug builds (Generic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    message(STATUS "Debug build detected: Enabling ASAN/UBSAN")
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fsanitize=address,undefined>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fno-omit-frame-pointer>)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

    # Automatically enable watermark checking in debug builds
    message(STATUS "Enabling stack watermark for Debug build")
    add_compile_definitions(FCONTEXT_ENABLE_STACK_WATERMARK=1)
endif()

# ============================================================================
# Include Directories
# ============================================================================

include_directories(include)

# ============================================================================
# Toolchain / Triple Configuration
# ============================================================================

if(NOT DEFINED TRIPLE)
    message(FATAL_ERROR "TRIPLE must be defined (e.g. -DTRIPLE=x86_64-pc-linux-gnu)")
endif()

message(STATUS "Loading configuration for triple: ${TRIPLE}")
include(toolchains/${TRIPLE}.cmake)

# ============================================================================
# Test Executables
# ============================================================================

set(TESTS
    test_fcontext_basic
    test_fcontext_simple
    test_fcontext_transfer
    test_fcontext_alignment
    test_fcontext_guard
    test_fcontext_watermark_mmap
    test_fcontext_watermark_malloc
)

foreach(test ${TESTS})
    add_executable(${test} tests/${test}.c src/fcontext_wrapper.c ${ASM_FILES})
endforeach()

# ============================================================================
# Testing
# ============================================================================

enable_testing()

# Ensure all test-related output goes to the build directory only
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED TRUE)

foreach(test ${TESTS})
    # Use absolute path to binary so tests run from build directory
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# test_fcontext_guard is expected to crash (segfault) due to stack overflow
set_tests_properties(test_fcontext_guard PROPERTIES WILL_FAIL TRUE)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Tests to build: ${TESTS}")
