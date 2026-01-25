# RISC-V 32-bit Support - Deferral Summary

**Decision Date:** 2026-01-25
**Status:** ⏸️ DEFERRED
**Owner:** Kyle Hayes

## Decision

RISC-V 32-bit (RV32) support has been deferred pending resolution of toolchain availability.

## Why Deferred

Verification testing (2026-01-25) determined that Debian's standard `gcc-riscv64-linux-gnu` package **does not include RV32 (ilp32) multilib support**.

### Test Results

**Verification completed on:** Debian Bookworm ARM64
**Test script:** `scripts/run_colima_riscv32_test.sh`
**Test log:** Available in `out/riscv32-test/riscv32_test.log`

| Component | Status | Notes |
|-----------|--------|-------|
| qemu-riscv32 | ✅ Available | Debian Bookworm includes qemu-riscv32 (v7.2.22) |
| gcc-riscv64-linux-gnu | ✅ Available | Debian Bookworm includes compiler (v12.2.0) |
| RV32 libraries | ❌ Missing | Missing `gnu/stubs-ilp32d.h` and `gnu/stubs-ilp32.h` |
| RV32 multilib support | ❌ Missing | Compiler lacks RV32-specific libc/libgcc |

### What Would Be Required to Re-enable

To support RV32, one of these approaches is needed:

**Option 1: Build Custom Toolchain (Recommended)**
- Build from [riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain) with RV32 support
- Adds 30-60 minutes to Docker build time
- Provides full control for embedded board customization
- Aligns with embedded RISC-V development practices

**Option 2: Use Pre-built Toolchain**
- Download pre-built RV32 toolchain in Docker
- Faster builds (5-10 minutes) but less control
- Dependent on external binary availability

**Option 3: Wait for Debian Package**
- Monitor [RISC-V/32 - Debian Wiki](https://wiki.debian.org/RISC-V/32)
- Potential future Debian package: `gcc-riscv32-linux-gnu`
- Low effort when available, but timeline uncertain

## What's Kept (Not Removed)

The following files and directories are **kept for future use** but are currently disabled:

### Documentation
- `RISCV32_IMPLEMENTATION_PLAN.md` - Marked as DEFERRED with complete implementation details
- `src/asm/riscv32/README.md` - Explains deferral status and re-enablement steps

### Test Scripts (DISABLED - not used)
- `scripts/test_riscv32_toolchain.sh` - Commented as disabled
- `scripts/run_colima_riscv32_test.sh` - Commented as disabled
- `out/riscv32-test/` - Test output directory from verification

### Empty Directories (Ready for implementation)
- `src/asm/riscv32/` - Assembly implementation directory (empty, awaiting files)

### Files NOT Created (Since deferred)
- `toolchains/riscv32-unknown-linux-gnu.cmake` - Not created
- `docker/Dockerfile.riscv32` - Not created
- `.github/workflows/riscv32-unknown-linux-gnu.yml` - Not created

## Current Active Support

No changes to active RISC-V support. RISC-V 64-bit (RV64) remains fully supported:
- ✅ `toolchains/riscv64-unknown-linux-gnu.cmake`
- ✅ `docker/Dockerfile.riscv64`
- ✅ `.github/workflows/riscv64-unknown-linux-gnu.yml`
- ✅ Assembly: `src/asm/riscv64/`

## To Re-enable RV32 Support

When ready to proceed with RV32:

1. **Review the plan:**
   ```bash
   cat RISCV32_IMPLEMENTATION_PLAN.md
   ```

2. **Choose toolchain approach:**
   - Option 1 (custom): Implement Dockerfile.riscv32 with source build
   - Option 2 (pre-built): Implement Dockerfile.riscv32 with binary download
   - Option 3 (wait): Monitor Debian packaging progress

3. **Implement Phase 1: Assembly**
   - Create `src/asm/riscv32/make_riscv32_sysv_elf_gas.S`
   - Create `src/asm/riscv32/jump_riscv32_sysv_elf_gas.S`
   - (Template/model available in the implementation plan)

4. **Implement Phase 2: Build Configuration**
   - Create `toolchains/riscv32-unknown-linux-gnu.cmake`
   - Create `docker/Dockerfile.riscv32`
   - Create `.github/workflows/riscv32-unknown-linux-gnu.yml`

5. **Testing (Phase 3)**
   - Uncomment and run: `bash scripts/run_colima_riscv32_test.sh`
   - Verify all tests pass with qemu-riscv32

6. **Documentation (Phase 4)**
   - Update `README.md` to add riscv32-unknown-linux-gnu to supported platforms
   - Update `src/asm/README.md` with RV32 details

## Key Learnings

1. **ABI Compatibility:** RV32 and RV64 ABIs are nearly identical - differences are register width and instruction variants (lw/sw vs ld/sd)

2. **Floating-Point Support:** Can be handled via preprocessor macros (`#if defined(__riscv_flen)`) to support both hard-float and soft-float builds

3. **Toolchain Maturity:** Standard Debian cross-compiler packages don't always include multilib support for less common architecture variants

4. **Embedded Need:** While deferred for now, RV32 is important for embedded systems (ESP32-C3, SiFive cores, etc.)

## References

- **Implementation Plan:** `RISCV32_IMPLEMENTATION_PLAN.md`
- **Status README:** `src/asm/riscv32/README.md`
- **Test Results:** `out/riscv32-test/riscv32_test.log`
- **RISC-V GNU Toolchain:** https://github.com/riscv-collab/riscv-gnu-toolchain
- **Debian RISC-V Status:** https://wiki.debian.org/RISC-V/32

## Next Review

Consider re-evaluating RISC-V 32-bit support when:
- Debian releases `gcc-riscv32-linux-gnu` package
- Specific embedded board support is requested (with estimated effort)
- Bandwidth becomes available for custom toolchain maintenance
