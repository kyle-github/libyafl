# Architecture Support Analysis

**Generated:** 2026-01-25
**Analysis of:** Build infrastructure, CI/CD workflows, and assembly implementations

## Summary

**Fully Supported:** 9 architectures
**Disabled:** 2 architectures (Xtensa/ESP32, RISC-V 32-bit)
**Partially Supported:** 10 architectures (ontop function removed)

---

## Architecture Support Matrix

### Tier 1: Complete Support

| Architecture | Triple | Toolchain | Workflow | ASM (make/jump) | ontop | Docker | Status |
|---|---|---|---|---|---|---|---|
| x86_64 | x86_64-pc-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **FULL** |
| ARM64 | aarch64-pc-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **FULL** |
| x86_64 | x86_64-apple-darwin | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| ARM64 | aarch64-apple-darwin | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| x86_64 | x86_64-pc-windows-msvc | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| ARM64 | aarch64-pc-windows-msvc | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| x86_64 | x86_64-pc-windows-gnu | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| ARM64 | aarch64-pc-windows-gnu | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |
| ARM64 | aarch64-apple-ios | ✅ | ✅ | ✅ | ❌ | ❌ | **FULL** |

### Tier 2: Full Core Support (No ontop)

These architectures have complete make/jump implementation but **no ontop function**. The CMakeLists.txt filters out ontop files, so builds succeed but ontop_fcontext API is unavailable.

| Architecture | Triple | Toolchain | Workflow | ASM (make/jump) | ontop | Docker | Status |
|---|---|---|---|---|---|---|---|
| x86_64 | x86_64-unknown-linux-android | ✅ | ✅ | ✅ | ❌ | ❌ | **PARTIAL** |
| ARM | arm-unknown-linux-gnueabihf | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| ARM64 | aarch64-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| RISC-V 64 | riscv64-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| MIPS32 | mipsel-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| MIPS64 | mips64el-unknown-linux-gnuabi64 | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| PowerPC32 | powerpc-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| PowerPC64 | powerpc64le-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |
| S390X | s390x-ibm-linux-gnu | ✅ | ✅ | ✅ | ❌ | ❌ | **PARTIAL** |
| SPARC64 | sparc64-unknown-linux-gnu | ✅ | ✅ | ✅ | ❌ | ✅ | **PARTIAL** |

### Tier 3: Disabled

| Architecture | Triple | Reason | Status |
|---|---|---|---|
| i386 | i386-unknown-linux-gnu | **BROKEN**: References deleted ontop files | ⚠️ BUILD ISSUE |
| Xtensa | xtensa-esp32-elf | No ASM implementation, disabled by user | ⏸️ DEFERRED |
| RISC-V 32 | riscv32-unknown-linux-gnu | Toolchain unavailable, deferred by user | ⏸️ DEFERRED |

---

## Critical Issues

### Issue 1: i386 (32-bit x86) - BUILD WILL FAIL

**Severity:** 🔴 CRITICAL

**Problem:**
- Toolchain file `toolchains/i386-unknown-linux-gnu.cmake` references ontop files that were deleted
- Ontop files are filtered out by CMakeLists.txt `list(FILTER ASM_FILES EXCLUDE REGEX "ontop")`
- **Result:** Build will fail because referenced files don't exist before filtering

**Files Referenced (Missing):**
- `src/asm/i386/ontop_i386_sysv_elf_gas.S`
- `src/asm/i386/ontop_i386_sysv_macho_gas.S`

**Build Impact:** If someone tries to build with `-DTARGET=i386-unknown-linux-gnu`, CMake will fail because the make/jump files reference the missing ontop file in the toolchain configuration.

**Solution Required:**
Either:
1. Remove ontop references from `toolchains/i386-unknown-linux-gnu.cmake`, OR
2. Restore the i386 ontop assembly files (see commit history)

---

## Missing ASM Files Analysis

### Files Deleted (Intentionally)

The following files were deleted in commit `4232b7a`:
> "Cleaning up unsupported code. The "ontop" functions needed special support on some platforms that was missing from the assembly. Removed those platforms."

**Deleted ontop files (23 total):**

**ARM (2 files):**
- `src/asm/arm/ontop_arm_aapcs_elf_gas.S`
- `src/asm/arm/ontop_arm_aapcs_macho_gas.S`

**ARM64 (3 files):**
- `src/asm/arm64/ontop_arm64_aapcs_elf_gas.S`
- `src/asm/arm64/ontop_arm64_aapcs_macho_gas.S`
- `src/asm/arm64/ontop_arm64_aapcs_pe_armasm.asm`
- `src/asm/arm64/ontop_arm64_aapcs_pe_armclang.S`

**i386 (2 files):**
- `src/asm/i386/ontop_i386_sysv_elf_gas.S`
- `src/asm/i386/ontop_i386_sysv_macho_gas.S`

**MIPS32 (1 file):**
- `src/asm/mips/ontop_mips32_o32_elf_gas.S`

**MIPS64 (1 file):**
- `src/asm/mips64/ontop_mips64_n64_elf_gas.S`

**PowerPC32 (1 file):**
- `src/asm/ppc32/ontop_ppc32_sysv_elf_gas.S`

**PowerPC64 (1 file):**
- `src/asm/ppc64/ontop_ppc64_sysv_elf_gas.S`

**RISC-V 64 (1 file):**
- `src/asm/riscv64/ontop_riscv64_sysv_elf_gas.S`

**S390X (1 file):**
- `src/asm/s390x/ontop_s390x_sysv_elf_gas.S`

**SPARC64 (1 file):**
- `src/asm/sparc64/ontop_sparc64_sysv_elf_gas.S`

**x86_64 (3 files):**
- `src/asm/x86_64/ontop_x86_64_ms_pe_gas.S`
- `src/asm/x86_64/ontop_x86_64_ms_pe_masm.asm`
- `src/asm/x86_64/ontop_x86_64_sysv_elf_gas.S`
- `src/asm/x86_64/ontop_x86_64_sysv_macho_gas.S`

### Files Never Implemented

**Xtensa (ESP32 - 3 files):**
- `src/asm/xtensa/make_xtensa_call0_sysv_elf_gas.S`
- `src/asm/xtensa/jump_xtensa_call0_sysv_elf_gas.S`
- `src/asm/xtensa/ontop_xtensa_call0_sysv_elf_gas.S`

**RISC-V 32 (0 files):**
- Directory exists but empty (deferred)

---

## Toolchain Files Referencing Missing Files

23 architectures have toolchain files that reference deleted or non-existent ontop files:

**Architecture Breakdown:**

| Architecture | Toolchains Affected | Missing File Count |
|---|---|---|
| ARM | 1 | 1 |
| ARM64 | 5 | 4 |
| i386 | 1 | 1 |
| MIPS32 | 1 | 1 |
| MIPS64 | 1 | 1 |
| PowerPC32 | 1 | 1 |
| PowerPC64 | 1 | 1 |
| RISC-V 64 | 1 | 1 |
| S390X | 1 | 1 |
| SPARC64 | 1 | 1 |
| x86_64 | 4 | 4 |
| **Xtensa** | **1** | **3** |
| **Total** | **21** | **23** |

---

## Platform-Specific Build Methods

### Platforms with Docker Support

Docker containers provided for cross-compilation on CI runners:

| Platform | Docker | Compiler | QEMU | Native Tests |
|---|---|---|---|---|
| ARM (32-bit) | ✅ Dockerfile.arm32 | gcc-arm-linux-gnueabihf | qemu-arm | ✅ |
| ARM64 | ✅ Dockerfile.aarch64 | gcc-aarch64-linux-gnu | qemu-aarch64 | ✅ |
| i386 | ✅ Dockerfile.i386 | gcc -m32 | qemu-i386 | ✅ |
| MIPS | ✅ Dockerfile.mips | gcc-mips-linux-gnu | qemu-mips(el) | ✅ |
| PowerPC | ✅ Dockerfile.ppc | gcc-powerpc-linux-gnu | qemu-ppc(64) | ✅ |
| RISC-V 64 | ✅ Dockerfile.riscv64 | gcc-riscv64-linux-gnu | qemu-riscv64 | ✅ |
| S390X | ❌ None | Installed directly | qemu-s390x | ✅ |
| SPARC64 | ✅ Dockerfile.sparc64 | gcc-sparc64-linux-gnu | qemu-sparc64 | ✅ |

### Platforms without Docker Support

These run directly on GitHub runners or with system-installed compilers:

| Platform | Method | Native Runner | Tests |
|---|---|---|---|
| x86_64 Linux | Native | ubuntu-latest | ✅ |
| x86_64 macOS | Native | macos-15-intel | ✅ |
| ARM64 Linux | Native | ubuntu-24.04-arm | ✅ |
| ARM64 macOS | Native | macos-15 | ✅ |
| Windows x86_64 MSVC | Native | windows-latest | ✅ |
| Windows x86_64 MinGW | Native | windows-latest | ✅ |
| Windows ARM64 | Native | windows-11-arm | ✅ |
| Android x86_64 | Emulator | ubuntu-latest | ✅ |
| iOS ARM64 | Simulator | macos-15 | ✅ |
| S390X Linux | System install + QEMU | ubuntu-24.04 | ✅ |

### Disabled/Broken Platforms

| Platform | Build Method | Status | Reason |
|---|---|---|---|
| Xtensa (ESP32) | Disabled | ⏸️ DEFERRED | No assembly implementation |
| RISC-V 32 | Disabled | ⏸️ DEFERRED | Toolchain unavailable (Debian lacks RV32 multilib) |

---

## Which Architectures Are NOT Fully Supported?

### 1. **i386 (32-bit x86)** - ⚠️ BUILD BROKEN

**Status:** Broken
**Issue:** Toolchain references deleted ontop files that CMake will try to find before filtering

**What's Missing:** ontop function implementation
**Affected Triples:** `i386-unknown-linux-gnu`
**Build Status:** ❌ WILL FAIL during CMake configuration

**Fix Required:**
```cmake
# In toolchains/i386-unknown-linux-gnu.cmake
# Remove these lines or restore the files:
    # src/asm/i386/ontop_i386_sysv_elf_gas.S
    # src/asm/i386/ontop_i386_sysv_macho_gas.S
```

---

### 2. **ARM (32-bit)** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `arm-unknown-linux-gnueabihf`
**Build Status:** ✅ Compiles (ontop filtered out by CMake)
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

**Details:**
- Core context switching works
- Stack frame creation works
- Advanced feature (ontop) not available
- Users cannot use three-way context switching

---

### 3. **ARM64 (AArch64)** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext (on all 5 platforms)

**Affected Triples:**
- `aarch64-apple-darwin` (macOS)
- `aarch64-pc-linux-gnu` (Linux GNU)
- `aarch64-pc-windows-msvc` (Windows MSVC)
- `aarch64-pc-windows-gnu` (Windows MinGW)
- `aarch64-unknown-linux-gnu` (Generic Linux)
- `aarch64-apple-ios` (iOS)

**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 4. **PowerPC32** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `powerpc-unknown-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 5. **PowerPC64LE** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `powerpc64le-unknown-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 6. **MIPS32** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `mipsel-unknown-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 7. **MIPS64** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `mips64el-unknown-linux-gnuabi64`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 8. **RISC-V 64** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `riscv64-unknown-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 9. **S390X** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `s390x-ibm-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 10. **SPARC64** - ⚠️ PARTIAL (ontop unavailable)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext

**Affected Triples:** `sparc64-unknown-linux-gnu`
**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 11. **x86_64** - ⚠️ PARTIAL (ontop unavailable on some platforms)

**Status:** Partial support
**Has:** make_fcontext, jump_fcontext
**Missing:** ontop_fcontext (on all platforms except native)

**Affected Triples:**
- `x86_64-pc-linux-gnu` (Linux - ontop filtered)
- `x86_64-apple-darwin` (macOS - ontop filtered)
- `x86_64-pc-windows-msvc` (Windows MSVC - ontop filtered)
- `x86_64-pc-windows-gnu` (Windows MinGW - ontop filtered)
- `x86_64-unknown-linux-android` (Android - ontop filtered)

**Build Status:** ✅ Compiles
**Runtime Status:** ⚠️ `ontop_fcontext` API unavailable

---

### 12. **Xtensa (ESP32)** - ⏸️ DISABLED

**Status:** Not implemented
**Reason:** No assembly implementation

**Affected Triples:** `xtensa-esp32-elf`
**Files Missing:** All (make, jump, ontop)
**Build Status:** ❌ Cannot compile (no ASM files)
**Runtime Status:** ❌ No support

**Note:** Marked as disabled by user. Kept for future implementation.

---

### 13. **RISC-V 32** - ⏸️ DISABLED

**Status:** Deferred
**Reason:** Toolchain unavailable (Debian lacks RV32 multilib support)

**Affected Triples:** `riscv32-unknown-linux-gnu`
**Build Status:** ⏸️ Not used
**Runtime Status:** ⏸️ Deferred

**Note:** Complete implementation plan available if needed in future.

---

## What the API Needs (For Reference)

### Available Functions

These are implemented on **ALL PLATFORMS** listed in Tier 1 and 2:

```c
// Create a context (ALWAYS AVAILABLE)
fcontext_t make_fcontext(void *sp, size_t size, void (*fn)(transfer_t));

// Transfer control between contexts (ALWAYS AVAILABLE)
transfer_t jump_fcontext(fcontext_t const to, void *vp);
```

### Unavailable Functions

These are **NOT AVAILABLE** on any platform except x86_64/ARM64 native builds (and even then, filtered):

```c
// Advanced context switching (DELETED)
transfer_t ontop_fcontext(fcontext_t const to, void *vp,
                          transfer_t (*fn)(transfer_t));
```

---

## Recommendations

### Priority 1: Fix i386 Build

**Issue:** Build will fail because CMakeLists.txt tries to load referenced files before filtering.

**Action Required:** Either:
1. Remove ontop file references from `toolchains/i386-unknown-linux-gnu.cmake`, OR
2. Restore deleted i386 ontop assembly files from git history

**Effort:** 15 minutes (updating toolchain file)

### Priority 2: Update Toolchain Documentation

**Issue:** Toolchain files reference ontop files that don't exist.

**Action:** Add comment to all affected toolchain files explaining that ontop is filtered out.

**Example:**
```cmake
# Note: ontop files are referenced below but are filtered out by CMakeLists.txt
# after loading this toolchain. Only make/jump functions are available.
```

**Affected Files:** 21 toolchain files
**Effort:** 30 minutes

### Priority 3: Document API Limitations

**Issue:** Users won't know `ontop_fcontext` is unavailable.

**Action:** Update README.md API section to note that `ontop_fcontext` is not available.

**Effort:** 15 minutes

### Priority 4: Consider Re-enabling ontop

**Option A (Low Cost):** Document that ontop requires special platform support.

**Option B (Higher Cost):** Restore ontop implementations for platforms where it's needed.

**Current Status:** Deleted intentionally with message "special support on some platforms that was missing."

---

## Summary Table

| Feature | i386 | ARM | ARM64 | MIPS32 | MIPS64 | PPC32 | PPC64 | RV64 | S390X | SPARC | x86_64 | Xtensa | RV32 |
|---------|------|-----|-------|--------|--------|-------|-------|------|-------|-------|--------|--------|------|
| make/jump | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| ontop | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Builds | ⚠️ BROKEN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ⏸️ |
| CI/Workflow | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⏸️ | ⏸️ |

---

## References

- Commit with ontop deletion: `4232b7a` ("Cleaning up unsupported code")
- RISC-V 32-bit deferral: `RISCV32_DEFERRAL_SUMMARY.md`
- Xtensa/ESP32 disabled: Disabled by user
- CMakeLists.txt filtering: Line 157 (`list(FILTER ASM_FILES EXCLUDE REGEX "ontop")`)
