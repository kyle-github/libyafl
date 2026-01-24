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

# Note: Assembly language will be enabled after architecture detection
# This allows us to set the correct compiler before enabling the language

# ============================================================================
# Compiler Flags
# ============================================================================

if(MSVC)
    # MSVC-specific flags - only for C/C++, not assembly
    add_compile_options($<$<COMPILE_LANGUAGE:C>:/W4>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:/WX>)
    # Debug info and optimization are handled by CMAKE_BUILD_TYPE
else()
    # GCC/Clang flags - only for C/C++, not assembly
    if(NOT CMAKE_C_FLAGS MATCHES "-Wall")
        add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wall>)
        add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wextra>)
        add_compile_options($<$<COMPILE_LANGUAGE:C>:-Werror>)
    endif()
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-g>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-O2>)
endif()

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
        # Choose assembly format based on compiler
        if(MSVC)
            # MSVC uses MASM syntax (.asm files)
            message(STATUS "Using MASM syntax for MSVC")
            set(ASM_FILES
                src/asm/x86_64/make_x86_64_ms_pe_masm.asm
                src/asm/x86_64/jump_x86_64_ms_pe_masm.asm
                src/asm/x86_64/ontop_x86_64_ms_pe_masm.asm
            )
        else()
            # MinGW/Clang use GAS syntax (.S files)
            message(STATUS "Using GAS syntax for MinGW/Clang")
            set(ASM_FILES
                src/asm/x86_64/make_x86_64_ms_pe_gas.S
                src/asm/x86_64/jump_x86_64_ms_pe_gas.S
                src/asm/x86_64/ontop_x86_64_ms_pe_gas.S
            )
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
        message(STATUS "Detected Windows on arm64 architecture")
        set(ARCH_TYPE "arm64")
        # Choose assembly format based on compiler
        if(MSVC)
            # MSVC uses ARMASM syntax (.asm files)
            message(STATUS "Using ARMASM syntax for MSVC")
            set(ASM_FILES
                src/asm/arm64/make_arm64_aapcs_pe_armasm.asm
                src/asm/arm64/jump_arm64_aapcs_pe_armasm.asm
                src/asm/arm64/ontop_arm64_aapcs_pe_armasm.asm
            )
        else()
            # MinGW/Clang use GAS syntax (.S files) with PE format
            message(STATUS "Using GAS/PE syntax for MinGW/Clang on ARM64")
            set(ASM_FILES
                src/asm/arm64/make_arm64_aapcs_pe_armclang.S
                src/asm/arm64/jump_arm64_aapcs_pe_armclang.S
                src/asm/arm64/ontop_arm64_aapcs_pe_armclang.S
            )
        endif()
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
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|RISCV64")
        message(STATUS "Detected Linux on riscv64 architecture")
        set(ARCH_TYPE "riscv64")
        set(ASM_FILES
            src/asm/riscv64/make_riscv64_sysv_elf_gas.S
            src/asm/riscv64/jump_riscv64_sysv_elf_gas.S
            src/asm/riscv64/ontop_riscv64_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch64|loongarch|LOONGARCH64")
        message(STATUS "Detected Linux on loongarch64 architecture")
        set(ARCH_TYPE "loongarch64")
        set(ASM_FILES
            src/asm/loongarch64/make_loongarch64_sysv_elf_gas.S
            src/asm/loongarch64/jump_loongarch64_sysv_elf_gas.S
            src/asm/loongarch64/ontop_loongarch64_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips|MIPS")
        # MIPS can be 32-bit or 64-bit; default to 32-bit (O32 ABI)
        message(STATUS "Detected Linux on mips architecture (32-bit)")
        set(ARCH_TYPE "mips")
        set(ASM_FILES
            src/asm/mips/make_mips32_o32_elf_gas.S
            src/asm/mips/jump_mips32_o32_elf_gas.S
            src/asm/mips/ontop_mips32_o32_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips64|MIPS64")
        message(STATUS "Detected Linux on mips64 architecture (64-bit)")
        set(ARCH_TYPE "mips64")
        set(ASM_FILES
            src/asm/mips64/make_mips64_n64_elf_gas.S
            src/asm/mips64/jump_mips64_n64_elf_gas.S
            src/asm/mips64/ontop_mips64_n64_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc|powerpc|PowerPC")
        # PowerPC can be 32-bit or 64-bit; determine from CMAKE_SYSTEM_PROCESSOR
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "64|ppc64|powerpc64")
            message(STATUS "Detected Linux on ppc64 architecture (64-bit)")
            set(ARCH_TYPE "ppc64")
            set(ASM_FILES
                src/asm/ppc64/make_ppc64_sysv_elf_gas.S
                src/asm/ppc64/jump_ppc64_sysv_elf_gas.S
                src/asm/ppc64/ontop_ppc64_sysv_elf_gas.S
            )
        else()
            message(STATUS "Detected Linux on ppc32 architecture (32-bit)")
            set(ARCH_TYPE "ppc32")
            set(ASM_FILES
                src/asm/ppc32/make_ppc32_sysv_elf_gas.S
                src/asm/ppc32/jump_ppc32_sysv_elf_gas.S
                src/asm/ppc32/ontop_ppc32_sysv_elf_gas.S
            )
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "s390x|S390X")
        message(STATUS "Detected Linux on s390x architecture")
        set(ARCH_TYPE "s390x")
        set(ASM_FILES
            src/asm/s390x/make_s390x_sysv_elf_gas.S
            src/asm/s390x/jump_s390x_sysv_elf_gas.S
            src/asm/s390x/ontop_s390x_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc64|SPARC64|sparcv9")
        message(STATUS "Detected Linux on sparc64 architecture")
        set(ARCH_TYPE "sparc64")
        set(ASM_FILES
            src/asm/sparc64/make_sparc64_sysv_elf_gas.S
            src/asm/sparc64/jump_sparc64_sysv_elf_gas.S
            src/asm/sparc64/ontop_sparc64_sysv_elf_gas.S
        )
    else()
        message(FATAL_ERROR "Unsupported architecture on Linux: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

else()
    message(FATAL_ERROR "Unsupported operating system: ${OS_TYPE}")
endif()

message(STATUS "Building for ${OS_TYPE} ${ARCH_TYPE}")

# ============================================================================
# Enable Assembly Language (after architecture detection)
# ============================================================================

if(WIN32 AND MSVC)
    # MSVC on Windows
    if(ARCH_TYPE STREQUAL "arm64")
        # ARM64 uses armasm64.exe - use custom build rules since CMake's
        # generic ASM support doesn't work well with armasm64
        find_program(ARMASM64 armasm64.exe REQUIRED)
        message(STATUS "Found ARM64 assembler: ${ARMASM64}")

        # Create custom commands for each ARM64 assembly file
        set(ARM64_ASM_OBJECTS "")
        foreach(asm_file ${ASM_FILES})
            get_filename_component(asm_name ${asm_file} NAME_WE)
            set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${asm_name}.obj")
            add_custom_command(
                OUTPUT ${obj_file}
                COMMAND ${ARMASM64} -machine ARM64 -o ${obj_file} ${CMAKE_CURRENT_SOURCE_DIR}/${asm_file}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${asm_file}
                COMMENT "Assembling ARM64 ${asm_file}"
                VERBATIM
            )
            list(APPEND ARM64_ASM_OBJECTS ${obj_file})
        endforeach()
        # Replace ASM_FILES with the object files for linking
        set(ASM_FILES ${ARM64_ASM_OBJECTS})
    else()
        # x86_64 uses ml64.exe (MASM)
        set(CMAKE_ASM_MASM_COMPILER ml64.exe CACHE FILEPATH "x64 MASM assembler" FORCE)
        enable_language(ASM_MASM)
        message(STATUS "Configuring for x86_64: using ml64.exe")
    endif()
else()
    # Unix-like systems OR MinGW/Clang on Windows: use GAS (GNU Assembler)
    enable_language(ASM)
endif()

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

# Set language properties for assembly files
# Note: ARM64 MSVC uses custom commands, so ASM_FILES contains .obj files, not .asm
if(MSVC AND WIN32 AND NOT ARCH_TYPE STREQUAL "arm64")
    foreach(asm_file ${ASM_FILES})
        if(asm_file MATCHES "\\.asm$")
            # x86_64 .asm files use ASM_MASM language with ml64
            set_source_files_properties(${asm_file} PROPERTIES LANGUAGE ASM_MASM)
        endif()
    endforeach()
endif()

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
