# CMake build configuration for fcontext
# Derived from Boost.Context - https://github.com/boostorg/context
# Copyright Oliver Kowalke (original), Kyle GV (2026, CMake changes)
# Distributed under the Boost Software License 1.0
# See LICENSE file for details

cmake_minimum_required(VERSION 3.10)
project(fcontext_tests C ASM)

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
# This applies to all build configurations
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Also set per-config output directories for multi-config generators
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES} Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG_TYPE} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
endforeach()

set(CMAKE_DEBUG_POSTFIX "")

# ============================================================================
# Language Configuration
# ============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_language(ASM)

# ============================================================================
# Compiler Flags
# ============================================================================

if(NOT CMAKE_C_FLAGS MATCHES "-Wall")
    add_compile_options(-Wall -Wextra -Werror)
endif()

add_compile_options(-g -O2)

# ============================================================================
# Include Directories
# ============================================================================

include_directories(include)

# ============================================================================
# Platform and Architecture Detection
# ============================================================================

# Detect OS
if(APPLE)
    set(OS_TYPE "macos")
elseif(UNIX AND NOT APPLE)
    set(OS_TYPE "linux")
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

# Detect and configure architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(ARCH_TYPE "x86_64")
    if(OS_TYPE STREQUAL "macos")
        set(ASM_FILES
            src/asm/x86_64/make_x86_64_sysv_macho_gas.S
            src/asm/x86_64/jump_x86_64_sysv_macho_gas.S
            src/asm/x86_64/ontop_x86_64_sysv_macho_gas.S
        )
    else()
        set(ASM_FILES
            src/asm/x86_64/make_x86_64_sysv_elf_gas.S
            src/asm/x86_64/jump_x86_64_sysv_elf_gas.S
            src/asm/x86_64/ontop_x86_64_sysv_elf_gas.S
        )
    endif()

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(ARCH_TYPE "arm64")
    if(OS_TYPE STREQUAL "macos")
        set(ASM_FILES
            src/asm/arm64/make_arm64_aapcs_macho_gas.S
            src/asm/arm64/jump_arm64_aapcs_macho_gas.S
            src/asm/arm64/ontop_arm64_aapcs_macho_gas.S
        )
    else()
        set(ASM_FILES
            src/asm/arm64/make_arm64_aapcs_elf_gas.S
            src/asm/arm64/jump_arm64_aapcs_elf_gas.S
            src/asm/arm64/ontop_arm64_aapcs_elf_gas.S
        )
    endif()

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
    set(ARCH_TYPE "i386")
    if(OS_TYPE STREQUAL "macos")
        set(ASM_FILES
            src/asm/i386/make_i386_sysv_macho_gas.S
            src/asm/i386/jump_i386_sysv_macho_gas.S
            src/asm/i386/ontop_i386_sysv_macho_gas.S
        )
    else()
        set(ASM_FILES
            src/asm/i386/make_i386_sysv_elf_gas.S
            src/asm/i386/jump_i386_sysv_elf_gas.S
            src/asm/i386/ontop_i386_sysv_elf_gas.S
        )
    endif()

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv7l")
    set(ARCH_TYPE "arm")
    if(OS_TYPE STREQUAL "macos")
        set(ASM_FILES
            src/asm/arm/make_arm_aapcs_macho_gas.S
            src/asm/arm/jump_arm_aapcs_macho_gas.S
            src/asm/arm/ontop_arm_aapcs_macho_gas.S
        )
    else()
        set(ASM_FILES
            src/asm/arm/make_arm_aapcs_elf_gas.S
            src/asm/arm/jump_arm_aapcs_elf_gas.S
            src/asm/arm/ontop_arm_aapcs_elf_gas.S
        )
    endif()

else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Building for ${OS_TYPE} ${ARCH_TYPE}")

# ============================================================================
# Test Executables
# ============================================================================

set(TESTS
    test_fcontext_basic
    test_fcontext_simple
    test_fcontext_transfer
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
    add_test(NAME ${test} COMMAND ${CMAKE_BINARY_DIR}/bin/${test})
endforeach()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Tests to build: ${TESTS}")
