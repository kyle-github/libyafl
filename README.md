# yafl - Portable Fiber/Coroutine Library

![x86_64-pc-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/x86_64-pc-linux-gnu.yml/badge.svg)
![aarch64-pc-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/aarch64-pc-linux-gnu.yml/badge.svg)
![x86_64-apple-darwin](https://github.com/libplctag/yafl/actions/workflows/x86_64-apple-darwin.yml/badge.svg)
![aarch64-apple-darwin](https://github.com/libplctag/yafl/actions/workflows/aarch64-apple-darwin.yml/badge.svg)
![x86_64-pc-windows-msvc](https://github.com/libplctag/yafl/actions/workflows/x86_64-pc-windows-msvc.yml/badge.svg)
![aarch64-pc-windows-msvc](https://github.com/libplctag/yafl/actions/workflows/aarch64-pc-windows-msvc.yml/badge.svg)
![x86_64-pc-windows-gnu](https://github.com/libplctag/yafl/actions/workflows/x86_64-pc-windows-gnu.yml/badge.svg)
![aarch64-pc-windows-gnu](https://github.com/libplctag/yafl/actions/workflows/aarch64-pc-windows-gnu.yml/badge.svg)
![x86_64-unknown-linux-android](https://github.com/libplctag/yafl/actions/workflows/x86_64-unknown-linux-android.yml/badge.svg)
![aarch64-apple-ios](https://github.com/libplctag/yafl/actions/workflows/aarch64-apple-ios.yml/badge.svg)
![arm-unknown-linux-gnueabihf](https://github.com/libplctag/yafl/actions/workflows/arm-unknown-linux-gnueabihf.yml/badge.svg)
![aarch64-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/aarch64-unknown-linux-gnu.yml/badge.svg)
![riscv64-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/riscv64-unknown-linux-gnu.yml/badge.svg)
![mipsel-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/mipsel-unknown-linux-gnu.yml/badge.svg)
![mips64el-unknown-linux-gnuabi64](https://github.com/libplctag/yafl/actions/workflows/mips64el-unknown-linux-gnuabi64.yml/badge.svg)
![powerpc64le-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/powerpc64le-unknown-linux-gnu.yml/badge.svg)
![s390x-ibm-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/s390x-ibm-linux-gnu.yml/badge.svg)
![sparc64-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/sparc64-unknown-linux-gnu.yml/badge.svg)
![powerpc-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/powerpc-unknown-linux-gnu.yml/badge.svg)
![i386-unknown-linux-gnu](https://github.com/libplctag/yafl/actions/workflows/i386-unknown-linux-gnu.yml/badge.svg)
[![codecov](https://codecov.io/gh/libplctag/yafl/graph/badge.svg)](https://codecov.io/gh/libplctag/yafl)

A portable, low-level C11 fiber/coroutine library derived from Boost.Context, designed to replace the deprecated `ucontext` API.

## Features

- **Pure C11 implementation** - No C++ dependencies
- **Asymmetric coroutines (fibers)** - Simplified suspend/resume model
- **Guard pages** - Memory-efficient mmap-based stack with automatic overflow detection
- **Stack watermark checking** - Measure maximum stack usage
- **16-byte stack alignment** - ABI-compliant on x86_64 and ARM64
- **Page-aware allocation** - Handles 4KB (Linux/Windows) and 16KB (macOS ARM) pages
- **Cross-platform** - Linux, macOS, iOS, Android, and Windows
- **Zero external dependencies** - Only standard APIs (POSIX/Windows)

## Quick Start

```c
#include "yafl.h"

static void *my_fiber_func(void *arg) {
    printf("Fiber running with arg: %p\n", arg);

    /* Suspend and wait for resumption */
    void *data = yafl_fiber_suspend((void *)0x1111);
    printf("Resumed with: %p\n", data);

    return (void *)0x2222;  /* Final result */
}

int main(void) {
    /* Create fiber with virtual memory stack and guard pages */
    yafl_fiber_t *fiber = yafl_fiber_create(
        my_fiber_func,
        16 * 1024,
        YAFL_STACK_FLAGS_VMEM | YAFL_STACK_FLAGS_WATERMARK
    );

    /* Start fiber */
    void *result = yafl_fiber_resume(fiber, (void *)42);
    assert(result == (void *)0x1111);  /* Got result from suspend */

    /* Resume fiber */
    result = yafl_fiber_resume(fiber, (void *)0x5555);
    assert(result == (void *)0x2222);  /* Got final result */
    assert(yafl_fiber_status(fiber) == YAFL_FIBER_STATUS_COMPLETE);

    /* Cleanup */
    yafl_fiber_destroy(fiber);
    return 0;
}
```

## API Overview

### Core Functions

```c
/* Creation with flags for stack type and watermark */
yafl_fiber_t *yafl_fiber_create(yafl_fiber_fn fiber_fn, size_t stack_size,
                                yafl_stack_flags_t flags);

/* Resume a fiber (start or continue) */
void *yafl_fiber_resume(yafl_fiber_t *fiber, void *arg);

/* Suspend current fiber */
void *yafl_fiber_suspend(void *result);

/* Query fiber status */
yafl_fiber_status_t yafl_fiber_status(yafl_fiber_t *fiber);

/* Get maximum stack usage (if watermarked) */
size_t yafl_fiber_stack_high_watermark(yafl_fiber_t *fiber);

/* Cleanup */
void yafl_fiber_destroy(yafl_fiber_t *fiber);

/* Utilities */
size_t yafl_get_page_size(void);
```

### Flags

Choose stack allocation type and optional watermark:

```c
/* Virtual memory (guard pages for overflow detection) */
YAFL_STACK_FLAGS_VMEM

/* Malloc (simple allocation, no guard pages) */
YAFL_STACK_FLAGS_MALLOC

/* Optional: track stack usage with watermark pattern */
YAFL_STACK_FLAGS_WATERMARK

/* Examples */
YAFL_STACK_FLAGS_VMEM                              /* vmem, no watermark */
YAFL_STACK_FLAGS_VMEM | YAFL_STACK_FLAGS_WATERMARK /* vmem + watermark */
YAFL_STACK_FLAGS_MALLOC                            /* malloc, no watermark */
YAFL_STACK_FLAGS_MALLOC | YAFL_STACK_FLAGS_WATERMARK
```

### Status Values

```c
YAFL_FIBER_STATUS_ERR        /* Invalid fiber or error */
YAFL_FIBER_STATUS_SUSPENDED  /* Waiting to be resumed */
YAFL_FIBER_STATUS_RUNNING    /* Currently executing */
YAFL_FIBER_STATUS_COMPLETE   /* Finished execution */
```

## Stack Options

### Virtual Memory Stacks (Recommended)

```c
yafl_fiber_t *fiber = yafl_fiber_create(
    my_func,
    16 * 1024,
    YAFL_STACK_FLAGS_VMEM
);
```

**Advantages:**
- Guard pages detect overflow/underflow
- Memory efficient (address space reserved, minimal physical memory used)
- Automatic bounds checking (SIGSEGV/access violation on overflow)

**Implementation:**
- Linux/macOS: Uses `mmap()` + `mprotect()` with PROT_NONE guard pages
- Windows: Uses `VirtualAlloc()` with PAGE_NOACCESS guard pages

### Malloc Stacks

```c
yafl_fiber_t *fiber = yafl_fiber_create(
    my_func,
    16 * 1024,
    YAFL_STACK_FLAGS_MALLOC
);
```

**Advantages:**
- Simple allocation without guard page overhead
- Useful for constrained environments

**Limitations:**
- No overflow detection
- Stack overflows cause undefined behavior

## Stack Watermarking

Enable with `YAFL_STACK_FLAGS_WATERMARK` flag:

```c
yafl_fiber_t *fiber = yafl_fiber_create(
    my_func,
    16 * 1024,
    YAFL_STACK_FLAGS_VMEM | YAFL_STACK_FLAGS_WATERMARK
);

/* After fiber execution */
size_t used = yafl_fiber_stack_high_watermark(fiber);
printf("Stack used: %zu bytes\n", used);
```

**How it works:**
1. Stack is filled with pattern `0xA5` at creation
2. As fiber executes, pattern is overwritten
3. On completion, scan detects how many bytes were used
4. Result: accurate measurement of maximum stack depth

**Overhead:**
- Negligible runtime cost (only at creation/destruction)
- Additional physical memory allocation (fills entire stack initially)

## Asymmetric Coroutines

This library implements asymmetric fibers - a fiber can only suspend back to its resumer.

```
      Main Thread
         |
      resume(fiber)
         |
         v
    [Fiber Running]
         |
      suspend()
         |
         v
      Main Thread
         |
      resume(fiber) again
         |
         v
    [Fiber Running Again]
         |
      return (complete)
         |
         v
      Main Thread
```

Not supported: Fiber A switching directly to Fiber B. Fibers always return to their resumer.

## Memory Layout

### Virtual Memory Stack

```
┌──────────────────────────┐
│ Guard Page (PROT_NONE)   │
├──────────────────────────┤
│ Stack Space (N pages)    │  Writable
│                          │  Grows downward
├──────────────────────────┤
│ Guard Page (PROT_NONE)   │
└──────────────────────────┘
```

**Address Space:** (N+2) × page_size
**Physical Memory:** Minimal (typically ~1 page initially)

### Malloc Stack

```
┌──────────────────────────┐
│ User-allocated block     │  No guard pages
│ (N + 256 bytes)          │  Simple heap allocation
└──────────────────────────┘
```

**Address Space:** N + 256 bytes
**Physical Memory:** Entire allocation

## Building

```bash
# Configure
cmake -B build

# Build
cmake --build build

# Test
cd build && ctest --output-on-failure
```

## Testing

Tests included:

- `test_yafl_basic` - Basic API functionality and flag combinations
- `test_yafl_suspend_resume` - Multiple suspend/resume cycles
- `test_yafl_guard` - Guard page overflow detection
- `test_yafl_many` - Scalability with 100 fibers

Run all tests:
```bash
cd build && ctest --output-on-failure
```

## Architecture Support

Tested on:
- x86_64 (Linux, macOS, Windows)
- ARM64 (Linux, macOS, iOS, Windows)
- ARM (Linux)
- RISC-V, MIPS, PowerPC (cross-compiled)

## Limitations

1. **Stack grows downward** - Required by implementation
2. **Entry function must use suspend/return** - Cannot return normally from fiber entry
3. **Not thread-safe** - Each thread needs its own fibers
4. **Asymmetric only** - No direct fiber-to-fiber switching

## License

Derived from Boost.Context, distributed under the Boost Software License 1.0.

See `LICENSE` file for details.

## References

- **Boost.Context**: https://github.com/boostorg/context
- **POSIX**: https://pubs.opengroup.org/onlinepubs/9699919799/
