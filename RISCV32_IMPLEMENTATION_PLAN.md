# RISC-V 32-bit Implementation Plan

## Status: ⏸️ DEFERRED

**Decision Date:** 2026-01-25

RV32 support has been deferred pending resolution of toolchain availability. The Debian package `gcc-riscv64-linux-gnu` does not include RV32 (ilp32) multilib support. Building a custom toolchain would add 30-60 minutes to Docker build times.

**To Re-enable in the future:**
1. Build custom RV32 toolchain from [riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain) in Docker
2. Uncomment the verification scripts in `scripts/`
3. Implement assembly files from Phase 1 of this plan
4. Follow remaining phases for full integration

---

## Executive Summary

**ABI Similarity:** Yes, RISC-V 32-bit (RV32) and 64-bit (RV64) ABIs are architecturally consistent. The same registers serve the same purposes; the primary difference is register width (32-bit vs 64-bit) and corresponding instruction changes.

**Toolchain Status:** ❌ **Not Available** - Verification completed. Debian's `gcc-riscv64-linux-gnu` package does not include RV32 multilib support. Missing libraries: `gnu/stubs-ilp32d.h`, `gnu/stubs-ilp32.h`

**Verification Results:** Test completed on Debian Bookworm ARM64 on 2026-01-25. See `scripts/run_colima_riscv32_test.sh` for details.

---

## Phase 0: Toolchain Verification (CRITICAL - DO THIS FIRST)

### Objective
Determine if we can build RV32 binaries using available Debian packages.

### Tasks

**Task 0.1: Run Verification Script**
```bash
cd /Users/kyle/Projects/fcontext
docker run --rm -v $(pwd):/src debian:bookworm-slim bash -c "\
  apt-get update && \
  apt-get install -y gcc-riscv64-linux-gnu qemu-user && \
  bash /src/scripts/test_riscv32_toolchain.sh"
```

**Expected Outcomes:**
- ✅ **Success**: Script exits 0 → Proceed to Phase 1
- ❌ **Failure**: Script exits 1 → See Alternative Approaches below

**Task 0.2: Document Findings**
Record which configuration works (if any):
- Compiler flags: `-march=rv32gc -mabi=ilp32d` (preferred) or alternatives
- Any limitations or workarounds needed

### Alternative Approaches if Verification Fails

**Option A: Build Custom Toolchain in Dockerfile**
```dockerfile
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y \
    cmake build-essential autoconf automake autotools-dev \
    curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk \
    bison flex texinfo gperf libtool patchutils bc zlib1g-dev \
    libexpat-dev qemu-user git

RUN git clone --recursive https://github.com/riscv-collab/riscv-gnu-toolchain && \
    cd riscv-gnu-toolchain && \
    ./configure --prefix=/opt/riscv32 --with-arch=rv32gc --with-abi=ilp32d && \
    make linux -j$(nproc)

ENV PATH="/opt/riscv32/bin:${PATH}"
WORKDIR /src
```
⚠️ **Warning**: This adds 30-60 minutes to Docker build time.

**Option B: Defer RV32 Support**
Document that RV32 support is pending toolchain availability and revisit when:
- Debian packages a dedicated riscv32 cross-compiler
- We have bandwidth to maintain custom toolchain builds
- Embedded users specifically request it

---

## Phase 1: Assembly Implementation

### Background: ABI Differences Between RV32 and RV64

**Similarities:**
- Same callee-saved registers: s0-s11 (x8-x9, x18-x27), fs0-fs11 (f8-f9, f18-f27)
- Same calling convention: a0-a7 for arguments, fa0-fa7 for FP arguments
- Same 16-byte stack alignment requirement
- Same register roles and preservation requirements

**Differences:**
| Aspect | RV32 | RV64 |
|--------|------|------|
| Register width | 32-bit | 64-bit |
| ABI name | ilp32/ilp32f/ilp32d | lp64/lp64f/lp64d |
| Integer load/store | `lw`/`sw` | `ld`/`sd` |
| FP load/store | `flw`/`fsw` (32-bit)<br>`fld`/`fsd` (64-bit) | `fld`/`fsd` |
| Context size | ~104 bytes (0x68) | ~208 bytes (0xd0) |
| Pointer size | 4 bytes | 8 bytes |

### Float ABI Detection

RISC-V defines preprocessor macros for compile-time detection:
```c
#if defined(__riscv_flen)
  // FPU hardware available
  #if __riscv_flen >= 64
    // Double-precision FPU (use ilp32d/lp64d)
  #elif __riscv_flen >= 32
    // Single-precision FPU (use ilp32f/lp64f)
  #endif
#else
  // Soft float (use ilp32/lp64)
#endif
```

**Strategy:** Create a single assembly file pair that conditionally saves/restores FP registers based on `__riscv_flen`, similar to ARM's `#if (defined(__VFP_FP__) && !defined(__SOFTFP__))`.

### Task 1.1: Create `src/asm/riscv32/make_riscv32_sysv_elf_gas.S`

**Context Layout for RV32:**
```
/*******************************************************
 *                                                     *
 *  -------------------------------------------------  *
 *  |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  *
 *  -------------------------------------------------  *
 *  | 0x0 | 0x4 | 0x8 | 0xc | 0x10| 0x14| 0x18| 0x1c|  *
 *  -------------------------------------------------  *
 *  |   fs0     |   fs1     |   fs2     |   fs3     |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  8  |  9  |  10 |  11 |  12 |  13 |  14 |  15 |  *
 *  -------------------------------------------------  *
 *  | 0x20| 0x24| 0x28| 0x2c| 0x30| 0x34| 0x38| 0x3c|  *
 *  -------------------------------------------------  *
 *  |   fs4     |   fs5     |   fs6     |   fs7     |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  16 |  17 |  18 |  19 |  20 |  21 |  22 |  23 |  *
 *  -------------------------------------------------  *
 *  | 0x40| 0x44| 0x48| 0x4c| 0x50| 0x54| 0x58| 0x5c|  *
 *  -------------------------------------------------  *
 *  |   fs8     |   fs9     |   fs10    |   fs11    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  24 |  25 |  26 |  27 |  28 |  29 |  30 |  31 |  *
 *  -------------------------------------------------  *
 *  | 0x60| 0x64| 0x68| 0x6c| 0x70| 0x74| 0x78| 0x7c|  *
 *  -------------------------------------------------  *
 *  |    s0     |    s1     |    s2     |    s3     |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  32 |  33 |  34 |  35 |  36 |  37 |  38 |  39 |  *
 *  -------------------------------------------------  *
 *  | 0x80| 0x84| 0x88| 0x8c| 0x90| 0x94| 0x98| 0x9c|  *
 *  -------------------------------------------------  *
 *  |    s4     |    s5     |    s6     |    s7     |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  40 |  41 |  42 |  43 |  44 |  45 |  46 |  47 |  *
 *  -------------------------------------------------  *
 *  | 0xa0| 0xa4| 0xa8| 0xac| 0xb0| 0xb4| 0xb8| 0xbc|  *
 *  -------------------------------------------------  *
 *  |    s8     |    s9     |   s10     |   s11     |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  48 |  49 |  50 |  51 |     |     |     |     |  *
 *  -------------------------------------------------  *
 *  | 0xc0| 0xc4| 0xc8| 0xcc|     |     |     |     |  *
 *  -------------------------------------------------  *
 *  |    ra     |    pc     |           |           |  *
 *  -------------------------------------------------  *
 *                                                     *
 *******************************************************/
```

**Total context size:** 0xd0 (208 bytes) - Same as RV64 for memory layout compatibility
- FP registers (fs0-fs11): 0x00-0x5f (96 bytes, 12×8 bytes for double-precision)
- GP registers (s0-s11): 0x60-0xbf (96 bytes, 12×8 bytes BUT only use lower 4 bytes each)
- Return address (ra): 0xc0 (4 bytes used)
- Program counter (pc): 0xc8 (4 bytes used)

**Implementation approach:**
```gas
.file "make_riscv32_sysv_elf_gas.S"
.text
.align  1
.global make_fcontext
.hidden make_fcontext
.type   make_fcontext, %function
make_fcontext:
    # shift address in a0 to lower 16 byte boundary
    andi a0, a0, ~0xF

    # reserve space for context-data on context-stack
    addi a0, a0, -0xd0

    # third arg == address of context-function
    # store address as PC to jump to
    sw  a2, 0xc8(a0)

    # save address of finish as return-address
    # will be entered after context-function returns (RA register)
    lla a4, finish
    sw  a4, 0xc0(a0)

    ret  # return pointer to context-data (a0)

finish:
    # exit code is zero
    li  a0, 0
    # exit application
    tail  _exit@plt

.size   make_fcontext,.-make_fcontext
.section .note.GNU-stack,"",%progbits
```

**Key changes from RV64:**
- `sd` → `sw` (store doubleword → store word)
- `ld` → `lw` (load doubleword → load word)
- Offsets remain the same for memory layout compatibility
- Instructions like `andi`, `addi`, `lla`, `li`, `ret`, `tail` work identically

### Task 1.2: Create `src/asm/riscv32/jump_riscv32_sysv_elf_gas.S`

**Implementation approach:**
```gas
.file "jump_riscv32_sysv_elf_gas.S"
.text
.align  1
.global jump_fcontext
.hidden jump_fcontext
.type   jump_fcontext, %function
jump_fcontext:
    # prepare stack for GP + FPU
    addi sp, sp, -0xd0

#if defined(__riscv_flen)
    # save fs0 - fs11 (always use double-precision stores for layout compatibility)
    fsd  fs0, 0x00(sp)
    fsd  fs1, 0x08(sp)
    fsd  fs2, 0x10(sp)
    fsd  fs3, 0x18(sp)
    fsd  fs4, 0x20(sp)
    fsd  fs5, 0x28(sp)
    fsd  fs6, 0x30(sp)
    fsd  fs7, 0x38(sp)
    fsd  fs8, 0x40(sp)
    fsd  fs9, 0x48(sp)
    fsd  fs10, 0x50(sp)
    fsd  fs11, 0x58(sp)
#endif

    # save s0-s11 (use sw for 32-bit registers)
    sw  s0, 0x60(sp)
    sw  s1, 0x68(sp)
    sw  s2, 0x70(sp)
    sw  s3, 0x78(sp)
    sw  s4, 0x80(sp)
    sw  s5, 0x88(sp)
    sw  s6, 0x90(sp)
    sw  s7, 0x98(sp)
    sw  s8, 0xa0(sp)
    sw  s9, 0xa8(sp)
    sw  s10, 0xb0(sp)
    sw  s11, 0xb8(sp)
    sw  ra, 0xc0(sp)

    # save RA as PC
    sw  ra, 0xc8(sp)

    # store SP (pointing to context-data) in A2
    mv  a2, sp

    # restore SP (pointing to context-data) from A0
    mv  sp, a0

#if defined(__riscv_flen)
    # load fs0 - fs11
    fld  fs0, 0x00(sp)
    fld  fs1, 0x08(sp)
    fld  fs2, 0x10(sp)
    fld  fs3, 0x18(sp)
    fld  fs4, 0x20(sp)
    fld  fs5, 0x28(sp)
    fld  fs6, 0x30(sp)
    fld  fs7, 0x38(sp)
    fld  fs8, 0x40(sp)
    fld  fs9, 0x48(sp)
    fld  fs10, 0x50(sp)
    fld  fs11, 0x58(sp)
#endif

    # load s0-s11, ra (use lw for 32-bit registers)
    lw  s0, 0x60(sp)
    lw  s1, 0x68(sp)
    lw  s2, 0x70(sp)
    lw  s3, 0x78(sp)
    lw  s4, 0x80(sp)
    lw  s5, 0x88(sp)
    lw  s6, 0x90(sp)
    lw  s7, 0x98(sp)
    lw  s8, 0xa0(sp)
    lw  s9, 0xa8(sp)
    lw  s10, 0xb0(sp)
    lw  s11, 0xb8(sp)
    lw  ra, 0xc0(sp)

    # return transfer_t from jump
    # pass transfer_t as first arg in context function
    # a0 == FCTX, a1 == DATA
    mv  a0, a2

    # load pc
    lw  a2, 0xc8(sp)

    # restore stack from GP + FPU
    addi sp, sp, 0xd0

    jr  a2
.size   jump_fcontext,.-jump_fcontext
.section .note.GNU-stack,"",%progbits
```

**Key changes from RV64:**
- Integer registers: `sd`/`ld` → `sw`/`lw`
- FP registers: Keep `fsd`/`fld` (double-precision for layout compatibility)
- Conditional compilation: `#if defined(__riscv_flen)` for FPU support
- Note: We use 8-byte spacing for integer registers too (storing at 0x60, 0x68, etc.) even though we only use 4 bytes. This maintains binary layout compatibility.

### Task 1.3: Alignment Consideration

**Critical:** We maintain the same 0xd0 (208-byte) context size and 8-byte stride layout for both RV32 and RV64. This decision:
- ✅ Simplifies code maintenance (same offsets)
- ✅ Allows binary compatibility in mixed environments
- ✅ Wastes only 52 bytes (4 bytes per 13 slots) per context
- ❌ Uses slightly more memory than minimal RV32 implementation

**Alternative:** Use a compact layout with 4-byte strides (total size 0x68 = 104 bytes). This saves memory but requires different offset calculations and breaks layout compatibility with RV64.

**Recommendation:** Use the 0xd0 layout initially for code simplicity. Optimize later if memory is critical for embedded use cases.

---

## Phase 2: Build Configuration

### Task 2.1: Create `toolchains/riscv32-unknown-linux-gnu.cmake`

```cmake
set(ASM_FILES
    src/asm/riscv32/make_riscv32_sysv_elf_gas.S
    src/asm/riscv32/jump_riscv32_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)
```

### Task 2.2: Create `docker/Dockerfile.riscv32`

```dockerfile
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    qemu-user \
    qemu-user-static \
    gcc-riscv64-linux-gnu \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
```

**Note:** If Phase 0 verification fails, replace with Dockerfile that builds custom toolchain (see Option A above).

### Task 2.3: Create `.github/workflows/riscv32-unknown-linux-gnu.yml`

```yaml
name: riscv32-unknown-linux-gnu
on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Build Docker image
        run: docker build -t fcontext-builder -f docker/Dockerfile.riscv32 .
      - name: Build and Test
        run: |
          docker run --rm -v ${{ github.workspace }}:/src fcontext-builder \
            bash -c "mkdir -p build && cd build && \
            cmake -DTARGET=riscv32-unknown-linux-gnu \
                  -DCMAKE_C_COMPILER=riscv64-linux-gnu-gcc \
                  -DCMAKE_C_FLAGS='-march=rv32gc -mabi=ilp32d -static' \
                  -DCMAKE_SYSTEM_NAME=Linux \
                  -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-riscv32 \
                  .. && \
            make && \
            CTEST_OUTPUT_ON_FAILURE=1 ctest"
```

**Variations to test:** If `-mabi=ilp32d` fails, try `-mabi=ilp32` or `-mabi=ilp32f`.

---

## Phase 3: Testing

### Task 3.1: Local Docker Build Test
```bash
cd /Users/kyle/Projects/fcontext
docker build -t fcontext-riscv32 -f docker/Dockerfile.riscv32 .
docker run --rm -v $(pwd):/src fcontext-riscv32 bash -c "\
  mkdir -p build && cd build && \
  cmake -DTARGET=riscv32-unknown-linux-gnu \
        -DCMAKE_C_COMPILER=riscv64-linux-gnu-gcc \
        -DCMAKE_C_FLAGS='-march=rv32gc -mabi=ilp32d -static' \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-riscv32 \
        .. && \
  make VERBOSE=1 && \
  CTEST_OUTPUT_ON_FAILURE=1 ctest"
```

### Task 3.2: Validation Checklist
- [ ] All test binaries compile without errors
- [ ] All tests pass under qemu-riscv32
- [ ] Stack alignment is correct (16-byte)
- [ ] Register preservation is correct (s0-s11, fs0-fs11 verified)
- [ ] Context switching works across multiple yields
- [ ] Stack watermark detection works (if enabled)
- [ ] No QEMU warnings or errors during execution

### Task 3.3: Comparison with RV64
Run identical tests on both RV32 and RV64 and compare:
- Test pass rates (should be 100% for both)
- Context sizes (0xd0 for both)
- Performance characteristics (context switch speed)

---

## Phase 4: Documentation

### Task 4.1: Update `src/asm/README.md`
Add RV32 to supported architectures list with ABI details:
```markdown
## RISC-V 32-bit (RV32)
- **Files:** `riscv32/make_riscv32_sysv_elf_gas.S`, `riscv32/jump_riscv32_sysv_elf_gas.S`
- **ABI:** ilp32d (hard-float double-precision, preferred) / ilp32 (soft-float)
- **Context Size:** 0xd0 (208 bytes)
- **Float Handling:** Conditional compilation based on `__riscv_flen`
- **Notes:** Uses same memory layout as RV64 for compatibility
```

### Task 4.2: Update `README.md`
Add to platform support matrix:
```markdown
| Platform | Triple | Status |
|----------|--------|--------|
| RISC-V 32-bit | riscv32-unknown-linux-gnu | ✅ Supported |
```

### Task 4.3: Document Embedded Variations
Create `docs/EMBEDDED_RISCV.md` for board-specific configurations:
```markdown
# RISC-V Embedded Configurations

## Standard Configurations
- **RV32GC (ilp32d):** General-purpose with hardware FPU (recommended)
- **RV32IMAC (ilp32):** Embedded without FPU (smaller footprint)

## Board-Specific Examples
- **ESP32-C3:** RV32IMC, ilp32 (no FPU)
- **SiFive E31:** RV32IMAC, ilp32
- **Future:** Add configurations as needed for specific embedded targets
```

---

## Embedded System Considerations

### Current Approach
The implementation supports multiple ABIs through conditional compilation:
1. **ilp32d** - Hardware double-precision FPU (general-purpose systems)
2. **ilp32f** - Hardware single-precision FPU (some embedded)
3. **ilp32** - Soft float (minimal embedded systems)

### Future Board-Specific Configurations
For specific embedded boards (ESP32-C3, SiFive cores, etc.), we can:
1. Create board-specific toolchain files (e.g., `toolchains/esp32c3.cmake`)
2. Set appropriate `-march` flags (e.g., `rv32imc` for ESP32-C3)
3. Document memory constraints and stack size recommendations
4. Provide example integration code for RTOS environments

### Memory-Optimized Variant
If 104 bytes of saved space per context matters for embedded:
1. Create `*_compact.S` variants with 0x68 layout
2. Use CMake option to select: `-DRISCV32_COMPACT_CONTEXT=ON`
3. Trade compatibility for memory efficiency

---

## Timeline and Effort Estimates

**Phase 0: Verification**
- Setup and run test: 30 minutes
- Analyze results: 30 minutes
- **Total: 1 hour**

**Phase 1: Assembly** (if verification succeeds)
- Create make_riscv32: 1 hour
- Create jump_riscv32: 1.5 hours
- Alignment verification: 30 minutes
- **Total: 3 hours**

**Phase 2: Build Config** (straightforward templating)
- Toolchain file: 15 minutes
- Dockerfile: 15 minutes
- GitHub workflow: 15 minutes
- **Total: 45 minutes**

**Phase 3: Testing** (most unpredictable)
- Local build test: 30 minutes
- Debug any failures: 2-4 hours (varies)
- Full test suite validation: 1 hour
- **Total: 3.5-5.5 hours**

**Phase 4: Documentation**
- Update READMEs: 30 minutes
- Create embedded docs: 30 minutes
- **Total: 1 hour**

**Grand Total: 9-11 hours** (assuming verification succeeds)

If custom toolchain build required (Option A): **Add 4-6 hours** for Dockerfile development and testing.

---

## Decision Points

### 1. Float ABI Strategy ✅ ANSWERED
**Question:** Support ilp32d (hard-float) vs ilp32 (soft-float)?

**Answer:** Support both via conditional compilation (`#if defined(__riscv_flen)`). The compiler flags (-mabi) determine which is used at build time. Users can build with either ABI as needed.

### 2. Toolchain Availability ⚠️ NEEDS VERIFICATION
**Question:** Can we use gcc-riscv64-linux-gnu for RV32, or build custom toolchain?

**Answer:** **Run Phase 0 verification first.** Decide based on results:
- If works: Proceed with standard Dockerfile
- If fails: Choose between custom toolchain build or defer support

### 3. Memory Layout 🤔 OPEN FOR DISCUSSION
**Question:** Use same 0xd0 layout as RV64, or optimize to 0x68 for RV32?

**Options:**
- **Option A (Recommended):** Use 0xd0 for code simplicity and compatibility
- **Option B:** Use 0x68 to save 104 bytes per context (better for embedded)

**Recommendation:** Start with Option A. Add Option B later if users request it for memory-constrained embedded systems.

---

## Next Steps

1. **Immediate:** Run Phase 0 verification script
   ```bash
   cd /Users/kyle/Projects/fcontext
   bash scripts/test_riscv32_toolchain.sh
   ```

2. **If verification succeeds:** Proceed to Phase 1 (Assembly implementation)

3. **If verification fails:**
   - Evaluate effort for custom toolchain (Option A)
   - Or defer RV32 support until tooling improves
   - Document decision in project README

4. **After implementation:** Create GitHub issue to track memory-optimized variant requests from embedded users

---

## References

- [RISC-V Calling Convention](https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc)
- [RISC-V Compiler Arguments](https://www.sifive.com/blog/all-aboard-part-1-compiler-args)
- [RISC-V Float ABI Detection](https://reviews.llvm.org/D60456)
- [Debian RISC-V 32-bit Wiki](https://wiki.debian.org/RISC-V/32)
- [RISC-V GNU Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)
- [QEMU RISC-V User Emulation](https://manpages.debian.org/testing/qemu-user/qemu-riscv32.1.en.html)
