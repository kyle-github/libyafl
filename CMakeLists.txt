# CMake build configuration for yafl
# Derived from Boost.Context - https://github.com/boostorg/context
# Copyright Oliver Kowalke (original), Kyle Hayes (2026, CMake changes)
# Distributed under the Boost Software License 1.0
# See LICENSE file for details

cmake_minimum_required(VERSION 3.10)

# Read version from file
set(PROJECT_VERSION_FULL "0.0.0-0")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VER_FILE_CONTENT)
    if(VER_FILE_CONTENT)
        string(STRIP "${VER_FILE_CONTENT}" VER_FILE_CONTENT_STRIPPED)
        if(VER_FILE_CONTENT_STRIPPED)
            set(PROJECT_VERSION_FULL "${VER_FILE_CONTENT_STRIPPED}")
        endif()
    endif()
endif()

# CMake project version must be numeric (major.minor.patch.tweak)
string(REPLACE "-" "." PROJECT_VERSION_CMAKE "${PROJECT_VERSION_FULL}")
project(yafl VERSION ${PROJECT_VERSION_CMAKE} LANGUAGES C)

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
    add_compile_definitions(yafl_ENABLE_STACK_WATERMARK=1)
endif()

# ============================================================================
# Include Directories
# ============================================================================

include_directories(include)

# ============================================================================
# Toolchain / Triple Configuration
# ============================================================================

if(DEFINED TARGET)
    set(TRIPLE "${TARGET}")
elseif(DEFINED TRIPLE)
    set(TRIPLE "${TRIPLE}")
else()
    # Auto-detect host triple
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(HOST_OS "linux")
        set(HOST_VENDOR "pc")
        set(HOST_ENV "gnu")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(HOST_OS "darwin")
        set(HOST_VENDOR "apple")
        set(HOST_ENV "")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(HOST_OS "windows")
        set(HOST_VENDOR "pc")
        if(MSVC)
            set(HOST_ENV "msvc")
        else()
            set(HOST_ENV "gnu")
        endif()
    else()
        message(FATAL_ERROR "Could not auto-detect platform. Please specify -DTARGET=<triple>")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(HOST_ARCH "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
        set(HOST_ARCH "aarch64")
    else()
        message(FATAL_ERROR "Could not auto-detect architecture. Please specify -DTARGET=<triple>")
    endif()

    if(HOST_ENV)
        set(TRIPLE "${HOST_ARCH}-${HOST_VENDOR}-${HOST_OS}-${HOST_ENV}")
    else()
        set(TRIPLE "${HOST_ARCH}-${HOST_VENDOR}-${HOST_OS}")
    endif()
    message(STATUS "Auto-detected host triple: ${TRIPLE}")
endif()

message(STATUS "Loading configuration for triple: ${TRIPLE}")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/${TRIPLE}.cmake")
    include(toolchains/${TRIPLE}.cmake)
else()
    message(FATAL_ERROR "Configuration for triple '${TRIPLE}' not found in toolchains/")
endif()

# ============================================================================
# Stack Direction Check
# ============================================================================

# We need to ensure the stack grows downward (high to low addresses).
# This is required by the assembly implementation.

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    message(STATUS "Skipping stack direction check for ${CMAKE_SYSTEM_NAME} (assuming downward)")
elseif(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    message(STATUS "Checking stack direction...")
    try_run(STACK_DIR_EXITCODE STACK_DIR_COMPILED
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_stack_direction.c"
    )

    if(STACK_DIR_COMPILED)
        if(STACK_DIR_EXITCODE EQUAL 0)
            message(STATUS "Stack grows downward (supported)")
        else()
            message(FATAL_ERROR "Stack grows upward! This architecture is not supported by yafl.")
        endif()
    else()
         message(WARNING "Failed to compile stack direction check. Assuming downward stack.")
    endif()
else()
    message(STATUS "Cross-compiling without emulator: Skipping stack direction check (assuming downward)")
endif()

# ============================================================================
# Test Executables
# ============================================================================

set(TESTS
    test_yafl_basic
    test_yafl_suspend_resume
    test_yafl_guard
    test_yafl_many
)

# Create static library
add_library(yafl STATIC src/yafl.c ${ASM_FILES})
target_include_directories(yafl PUBLIC include)
set_target_properties(yafl PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

foreach(test ${TESTS})
    add_executable(${test} tests/${test}.c)
    target_link_libraries(${test} PRIVATE yafl)
    # Link dl library for test_yafl_guard on Linux/Unix for dladdr
    if("${test}" STREQUAL "test_yafl_guard" AND UNIX)
        target_link_libraries(${test} PRIVATE ${CMAKE_DL_LIBS})
    endif()
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

# test_yafl_guard is expected to crash (segfault) due to stack overflow
# set_tests_properties(test_yafl_guard PROPERTIES WILL_FAIL TRUE)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Tests to build: ${TESTS}")

# Target to update Boost.Context assembly files
# Usage: cmake -DBOOST_VERSION=1.81.0 .. && make update_boost
add_custom_target(update_boost
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_boost.sh ${BOOST_VERSION}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating Boost.Context assembly files"
)
