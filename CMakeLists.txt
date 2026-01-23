# CMake build configuration for fcontext
# Derived from Boost.Context - https://github.com/boostorg/context
# Copyright Oliver Kowalke (original), Kyle Hayes (2026, CMake changes)
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
elseif(WIN32)
    set(OS_TYPE "windows")
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

# Detect architecture and select appropriate assembly files based on OS + architecture
# Note: Only check supported combinations for each OS
if(OS_TYPE STREQUAL "macos")
    # macOS only supports 64-bit architectures
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        message(STATUS "Detected macOS on x86_64 architecture")
        set(ARCH_TYPE "x86_64")
        set(ASM_FILES
            src/asm/x86_64/make_x86_64_sysv_macho_gas.S
            src/asm/x86_64/jump_x86_64_sysv_macho_gas.S
            src/asm/x86_64/ontop_x86_64_sysv_macho_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
        message(STATUS "Detected macOS on arm64 architecture")
        set(ARCH_TYPE "arm64")
        set(ASM_FILES
            src/asm/arm64/make_arm64_aapcs_macho_gas.S
            src/asm/arm64/jump_arm64_aapcs_macho_gas.S
            src/asm/arm64/ontop_arm64_aapcs_macho_gas.S
        )
    else()
        message(FATAL_ERROR "Unsupported architecture on macOS: ${CMAKE_SYSTEM_PROCESSOR} (macOS only supports x86_64 and ARM64)")
    endif()

elseif(OS_TYPE STREQUAL "windows")
    # Windows only supports 64-bit architectures
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        message(STATUS "Detected Windows on x86_64 architecture")
        set(ARCH_TYPE "x86_64")
        set(ASM_FILES
            src/asm/x86_64/make_x86_64_ms_pe_gas.S
            src/asm/x86_64/jump_x86_64_ms_pe_gas.S
            src/asm/x86_64/ontop_x86_64_ms_pe_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
        message(STATUS "Detected Windows on arm64 architecture")
        set(ARCH_TYPE "arm64")
        set(ASM_FILES
            src/asm/arm64/make_arm64_aapcs_pe_armasm.asm
            src/asm/arm64/jump_arm64_aapcs_pe_armasm.asm
            src/asm/arm64/ontop_arm64_aapcs_pe_armasm.asm
        )
    else()
        message(FATAL_ERROR "Unsupported architecture on Windows: ${CMAKE_SYSTEM_PROCESSOR} (Windows only supports x86_64 and ARM64)")
    endif()

elseif(OS_TYPE STREQUAL "linux")
    # Linux supports both 32-bit and 64-bit architectures
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        message(STATUS "Detected Linux on x86_64 architecture")
        set(ARCH_TYPE "x86_64")
        set(ASM_FILES
            src/asm/x86_64/make_x86_64_sysv_elf_gas.S
            src/asm/x86_64/jump_x86_64_sysv_elf_gas.S
            src/asm/x86_64/ontop_x86_64_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
        message(STATUS "Detected Linux on arm64 architecture")
        set(ARCH_TYPE "arm64")
        set(ASM_FILES
            src/asm/arm64/make_arm64_aapcs_elf_gas.S
            src/asm/arm64/jump_arm64_aapcs_elf_gas.S
            src/asm/arm64/ontop_arm64_aapcs_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
        message(STATUS "Detected Linux on i386 architecture")
        set(ARCH_TYPE "i386")
        set(ASM_FILES
            src/asm/i386/make_i386_sysv_elf_gas.S
            src/asm/i386/jump_i386_sysv_elf_gas.S
            src/asm/i386/ontop_i386_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv7l")
        message(STATUS "Detected Linux on arm architecture")
        set(ARCH_TYPE "arm")
        set(ASM_FILES
            src/asm/arm/make_arm_aapcs_elf_gas.S
            src/asm/arm/jump_arm_aapcs_elf_gas.S
            src/asm/arm/ontop_arm_aapcs_elf_gas.S
        )
    else()
        message(FATAL_ERROR "Unsupported architecture on Linux: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

else()
    message(FATAL_ERROR "Unsupported operating system: ${OS_TYPE}")
endif()

message(STATUS "Building for ${OS_TYPE} ${ARCH_TYPE}")

# ============================================================================
# Test Executables
# ============================================================================

set(TESTS
    test_fcontext_basic
    test_fcontext_simple
    test_fcontext_transfer
    test_fcontext_watermark
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
