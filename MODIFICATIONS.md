# Modifications from Boost.Context

This document tracks all modifications made to code derived from Boost.Context to avoid losing these changes when updating from upstream.

## API Changes

### Assembly Function Renaming
**All architectures**

**Original (Boost.Context):**
- Low-level functions: `make_fcontext` and `jump_fcontext`

**Modified (fcontext):**
- Renamed to: `make_fcontext` and `jump_fcontext`
- Symbol name changes in all assembly files (make_*.S and jump_*.S)

**Files affected:**
- All assembly files across all architectures
- Function symbols updated in .globl declarations and labels

### Removal of ontop_fcontext
**All architectures**

**Original (Boost.Context):**
- Included `ontop_fcontext` assembly functions for continuation semantics

**Modified (fcontext):**
- All `ontop_fcontext` functions completely removed
- Simplified API with only context initialization and switching

**Files affected:**
- All `ontop_*.S` files deleted
- CMakeLists.txt and all toolchain files (removed references)
- CI/YAML workflows (removed references)

### Fiber Function Signatures
**All architectures**

**Original (Boost.Context):**
- Fiber functions returned `fcontext_transfer_t`
- Required explicit `jump_fcontext()` call at end to return control

**Modified (fcontext):**
- Fiber functions now return `void`
- Fiber functions can return naturally without explicit context switch
- Return instruction triggers automatic return to caller via `finish` routine

**Files affected:**
- All assembly files (make_*.S files across all architectures)
- Tests: All test files (test_fcontext_*.c)

## Architecture-Specific Modifications

### MIPS64 (mips64/make_mips64_n64_elf_gas.S and mips64/jump_mips64_n64_elf_gas.S)

**Change: Removed $gp (Global Pointer) save/restore**

**Rationale:**
- $gp is shared across the entire program
- No need to save/restore per context
- Simplified assembly and eliminated a source of complexity

**Specific changes:**
1. `make_mips64_n64_elf_gas.S` line ~76: Removed `sd $gp, 136($v0)`
2. `finish` routine: Removed `ld $gp, 136($s1)` and now uses program's shared $gp with `dla` pseudo-instruction

**When updating from Boost.Context:**
- If MIPS64 files are updated, remove the $gp save in context initialization
- Remove the $gp restore in the finish routine
- See `src/asm/mips64/README.md` for detailed explanation

### MIPS32 (mips/make_mips32_o32_elf_gas.S)

**Status:** Not modified yet, but verify if similar $gp handling is needed.

## Stack Management Changes

### All architectures

**Change: Metadata positioning and stack alignment**

**Files affected:**
- `src/fcontext_wrapper.c`: fcontext_malloc_stack() and fcontext_vmem_stack()

**Details:**
- Metadata is now stored at the highest address of usable stack (stack_top)
- 256 bytes of overhead allocated before rounding to page boundary
- Ensures 16-byte alignment compliance for all functions
- Not part of Boost.Context - this is new fcontext functionality

## Windows-Specific Modifications

### fcontext_vmem_stack() - VirtualAlloc strategy

**File:** `src/fcontext_wrapper.c`

**Change:** Modified approach to page allocation:
1. Reserve entire region as PAGE_NOACCESS
2. Commit only stack portion as PAGE_READWRITE
3. Guard pages remain reserved but inaccessible

**Previous approach:** Allocated all as PAGE_READWRITE, then tried to change protection

**Rationale:** Avoids overlapping protection states and more closely aligns with mmap/mprotect semantics on POSIX

**Not from Boost.Context** - this is fcontext-specific Windows implementation

## Updating from Boost.Context

When pulling updates from Boost.Context, follow this checklist:

1. **Function naming**: Ensure assembly functions are named `make_fcontext` and `jump_fcontext` (not `make_fcontext`/`jump_fcontext`)
2. **Remove ontop functions**: Do not include any `ontop_*.S` files or references
3. **Check MIPS64 $gp handling**: Remove $gp save/restore if present
4. **Verify stack direction check**: Ensure Android is included in CMakeLists.txt skip list
5. **Update function entry points**: Ensure fiber functions are called with proper signatures and can return naturally
6. **Review any new assembly files**: Check for similar patterns to known issues
7. **Test all platforms thoroughly** after updates
8. **Update this document** if new modifications are needed

## Common Pitfalls When Updating

- **Forgetting to rename functions**: `make_fcontext` → `make_fcontext`, `jump_fcontext` → `jump_fcontext`
- **Keeping ontop files**: Ensure all `ontop_*.S` are excluded and references removed from CMake
- **Re-introducing $gp save/restore on MIPS64**: Watch for this in updated MIPS64 files
- **Changing stack alignment logic**: Ensure 256-byte overhead and metadata positioning is preserved

## Testing

All modifications are validated by the test suite in `tests/`:
- test_fcontext_basic
- test_fcontext_simple
- test_fcontext_transfer
- test_fcontext_alignment
- test_fcontext_guard
- test_fcontext_watermark_mmap
- test_fcontext_watermark_malloc
- test_fcontext_low_level
