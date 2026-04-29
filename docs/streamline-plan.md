# Streamlining Summary

## Summary

The coroutine-layer streamlining work is implemented across the C runtime, assembly backends, and toolchain selection files. YAFL now uses `yafl_switch(yafl_t *save, yafl_t target, void *data)` and `yafl_make_context()` internally, which removes `transfer_t`, removes `pending_arg`, and keeps the hot path at a save/restore plus direct data handoff.

The public API in `include/yafl.h` stays unchanged. All of the streamlining work remains below that surface.

## Status

- `src/yafl.c` now uses direct `void *` handoff through `yafl_switch`.
- Assembly sources and exported symbols are renamed to `switch_*` / `make_context_*` and `yafl_switch` / `yafl_make_context`.
- Toolchain `ASM_FILES` lists point at the renamed files.
- The host arm64 build and test suite pass after the refactor.

## Implemented Design

### Low-level switch primitive

The internal switch ABI is now:

`void *yafl_switch(yafl_t *save, yafl_t target, void *data)`

- `*save` receives the current suspended context.
- `target` is the context to resume.
- `data` is delivered directly both as the resumed return value and as the first-entry argument.

This removes the synthetic transport struct and the Windows-specific hidden struct-return plumbing that existed in the old backend model.

### Initial context creation

The initial saved context is created by:

`yafl_t yafl_make_context(void *sp, size_t size, yafl_entry_t fn)`

Each backend-specific `make_context_*` file prepares the initial stack/register image for a new fiber and installs the `finish:` fallback path that terminates the process if a fiber entry function unexpectedly returns through the raw frame.

### C runtime changes

- `yafl_transfer_t` is removed.
- `pending_arg` is removed from `struct yafl_fiber`.
- `fiber_entry_trampoline()` now receives the first resume argument directly and discovers the active fiber through TLS.
- `yafl_fiber_resume()` and `yafl_fiber_suspend()` use direct `void *` handoff through `yafl_switch()`.
- Fiber stack cleanup and watermark reinitialization are both routed through helpers so failure paths stay linear.

### Assembly and naming changes

- All internal switch-family files are renamed from `jump_*` to `switch_*`.
- All initial-context files are renamed from `make_*` to `make_context_*`.
- Exported assembly symbols are renamed from `jump_fcontext` / `make_fcontext` to `yafl_switch` / `yafl_make_context`.
- Toolchain `ASM_FILES` lists in `toolchains/*.cmake` point at the renamed files.

### Backend scope

The direct-data `yafl_switch` model is implemented across the maintained backends in the tree, including x86_64, arm64, i386, arm, riscv64, loongarch64, mips32, mips64, ppc32, ppc64, s390x, and sparc64.

The guiding backend rule is unchanged across architectures: save-via-pointer, restore-target-from-second-argument, and pass `data` directly.

## Validation

- Host arm64 library build succeeds.
- Host tests pass: `test_yafl_basic`, `test_yafl_suspend_resume`, `test_yafl_guard`, `test_yafl_many`, and `test_yafl_watermark`.
- Maintained sources no longer carry live `transfer_t`, `pending_arg`, `jump_fcontext`, or `make_fcontext` implementation references.
- Non-host assembly validation is opportunistic and depends on the locally available target assemblers.

## Remaining Follow-up

- Keep generated coverage artifacts in sync with the source tree when coverage is regenerated.
- Expand cross-target validation when additional target toolchains are available.

## Decisions

1. **Do not move the C trampoline into assembly**
   - That would hardcode `struct yafl_fiber` offsets into every backend.
   - The resulting code would be harder to audit and not meaningfully faster.

2. **Do not collapse the initial-context primitive and the switch primitive into one assembly entry point**
   - `yafl_switch` is the hot path.
   - `yafl_make_context` runs only during fiber creation.
   - Combining them would trade a tiny amount of source reduction for a branchier and less legible hot path.

3. **Do not merge ELF and Mach-O assembly files even when they currently match**
   - OS-level context requirements may diverge later, including support for wider architectural state such as AVX or other OS-managed register state.
   - Keep separate per-OS source files so those changes can land without re-splitting shared assembly later.

4. **Do not introduce runtime architecture dispatch to reduce file count**
   - The current per-target selection in the toolchain files keeps the build explicit and predictable.
   - Runtime selection would increase complexity without helping performance.

## Relevant Files

- `src/yafl.c` — current C runtime implementation of the streamlined switch semantics.
- `include/yafl.h` — no public API changes are required.
- `src/asm/*/switch_*` — backend switch-family sources implementing `yafl_switch`.
- `src/asm/*/make_context_*` — backend entry-context sources implementing `yafl_make_context`.
- `toolchains/*.cmake` — backend selection for the renamed assembly sources.
