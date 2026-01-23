# fcontext - Portable Context Switching Library

A portable, low-level C11 fiber/coroutine context switching library derived from Boost.Context, designed to replace the deprecated and unsupported `ucontext` API.

## Features

- **Pure C11 implementation** - No C++ dependencies, no platform-specific quirks
- **Symmetric coroutines** - Stackful green threads/coroutines that yield control to each other
- **Guard pages** - Memory-efficient mmap-based stack with automatic bounds detection
- **Stack watermark checking** - Detects high water mark (maximum stack usage) with 0xA5 pattern
- **16-byte stack alignment** - Automatic ABI-compliant alignment for x86_64 and ARM64
- **Page-aware allocation** - Automatically handles varying page sizes (4KB Linux, 16KB macOS ARM, etc.)
- **Portable across platforms** - x86_64 and ARM64 on Linux, macOS, and Windows
- **Portable across architectures** - x86_64, ARM64, x86, ARM support
- **Zero external dependencies** - Only standard APIs (POSIX mmap on Linux/macOS, Windows VirtualAlloc on Windows)

## Why This Exists

The POSIX `ucontext` API is:
- **Deprecated** on most modern operating systems
- **Broken on macOS Aarch64** (Apple Silicon) - no native support
- **No longer maintained** - new POSIX standard removed it in 2008.

This library replaces `ucontext`.

## Architecture Support

### Tier 1 - Fully Tested (Native & Cross-Compiled)

| Architecture | Linux | macOS | Windows |
|:---|:---:|:---:|:---:|
| x86_64 (AMD64) | ✓ | ✓ | ✓ |
| ARM64 (AArch64) | ✓ | ✓ (Apple Silicon) | ✓ |
| x86 (i386) | ✓ | ✓ | - |
| ARM (32-bit) | ✓ | ✓ | - |

**Note:** 32-bit architectures not supported on Windows (modern Windows is 64-bit only).

Page sizes automatically detected:
- **Linux/Windows**: 4KB (typical)
- **macOS ARM64**: 16KB (Apple Silicon requirement)
- **Page size detection**: Via `sysconf(_SC_PAGE_SIZE)` on POSIX, `GetSystemInfo()` on Windows

### Continuous Integration Testing

All combinations below are automatically tested on each push via GitHub Actions:

**Native Builds:**
| Job | Runner | OS | Architecture |
|-----|--------|-------|--------------|
| build-and-test | ubuntu-latest | Ubuntu | AMD64 |
| build-and-test | ubuntu-24.04-arm | Ubuntu | ARM64 |
| build-and-test | macos-15-intel | macOS | AMD64 |
| build-and-test | macos-15 | macOS | ARM64 |
| build-and-test | windows-latest | Windows | AMD64 |
| build-and-test | windows-11-arm | Windows | ARM64 |

**Cross-Compiled Builds (with QEMU):**
| Job | Runner | OS | Architecture |
|-----|--------|-------|--------------|
| cross-compile-i386 | ubuntu-24.04 | Ubuntu | i386 (32-bit x86) |
| cross-compile-armhf | ubuntu-24.04 | Ubuntu | armhf (32-bit ARM) |

**Note:** Cross-compiled builds use separate jobs with dedicated toolchain setup. The i386 packages come from the main Ubuntu archive, while armhf packages require the Ubuntu ports repository (ports.ubuntu.com).

## API Overview

### Two-Layer Design

1. **Low-Level API** (`make_fcontext`, `jump_fcontext`, `ontop_fcontext`)
   - Written in architecture/OS-specific assembly.

2. **High-Level Convenience API** (`fcontext_create`, `fcontext_destroy`)
   - Handles memory allocation and guard pages automatically.
   - Recommended for most use cases.

## Low-Level API

### Core Functions

From `fcontext.h`.

```c
/* Create a context at a given stack location */
fcontext_t make_fcontext(void *sp, size_t size, fcontext_fn_t fn);

/* Switch to a context */
fcontext_transfer_t jump_fcontext(fcontext_t const to, void *vp);

/* Call function on top of a context */
fcontext_transfer_t ontop_fcontext(fcontext_t const to, void *vp,
                                    fcontext_ontop_fn_t fn);
```

### Types

```c
/* Opaque context handle */
typedef struct fcontext_opaque_t *fcontext_t;

/* Data transferred on context switch */
typedef struct {
    fcontext_t prev_context;  /* Where we came from */
    void *data;               /* User-provided data */
} fcontext_transfer_t;

/* Entry point function signature */
typedef void (*fcontext_fn_t)(fcontext_transfer_t);

/* Function for ontop_fcontext */
typedef fcontext_transfer_t (*fcontext_ontop_fn_t)(fcontext_transfer_t);
```

### Low-Level Example

```c
#include <stdio.h>
#include <stdlib.h>
#include "fcontext.h"

void fiber_entry(fcontext_transfer_t t) {
    printf("Fiber executing\n");
    /* Switch back to caller */
    jump_fcontext(t.prev_context, NULL);
}

int main(void) {
    /* Allocate 24KB stack */
    size_t stack_size = 24 * 1024;
    void *stack = malloc(stack_size);

    /* Create context at top of stack */
    fcontext_t ctx = make_fcontext(
        (char *)stack + stack_size,  /* Stack pointer (top of stack) */
        stack_size,                   /* Stack size */
        fiber_entry                   /* Entry function */
    );

    /* Enter context */
    fcontext_transfer_t t = jump_fcontext(ctx, NULL);

    /* When execution returns here, fiber has completed */
    printf("Back in main\n");

    free(stack);
    return 0;
}
```

## High-Level Convenience API

### Functions

Again, from `fcontext.h`.

```c
/* Get system page size (4KB, 16KB, etc.) */
size_t fcontext_get_page_size(void);

/* Round size up to nearest page boundary */
size_t fcontext_align_to_page(size_t size);

/* Create context with automatically allocated guarded stack */
fcontext_stack_t *fcontext_create(size_t stack_size, fcontext_fn_t entry_fn);

/* Destroy context and free stack */
void fcontext_destroy(fcontext_stack_t *ctx);

/* Switch to context */
#define fcontext_swap(ctx, data) jump_fcontext((ctx)->context, (data))
```

### Stack Layout

The high-level API allocates stack with guard pages to detect overflow:

```
Address Space Layout:
┌─────────────────────┐
│  Guard Page         │  (protected, will fault on access)
├─────────────────────┤
│  Actual Stack       │  (mapped, readable/writable)
│  (one+ pages)       │  Size: rounded to page boundary
├─────────────────────┤
│  Guard Page         │  (protected, will fault on access)
└─────────────────────┘

Total Allocation = page_size + round_up(stack_size) + page_size (address space)
Physical Memory  = 1 page (only one page of the stack is physically resident)
```

**Allocation method by platform:**
- **Linux/macOS**: `mmap()` + `mprotect()` (POSIX standard)
- **Windows**: `VirtualAlloc()` + `VirtualProtect()` (Windows API)

Benefits:
- **Automatic overflow detection** - Stack overflow causes segmentation fault (all platforms)
- **Memory efficient** - Guard pages use address space, not physical memory
- **Page-aware** - Works correctly with 4KB (Linux/Windows) and 16KB (macOS ARM) pages

### Stack Watermark Checking

When enabled (default), the entire stack is filled with pattern `0xA5` at creation. When the context is destroyed, the library scans from the bottom upward to detect the high water mark:

```c
/* Create context with watermark checking */
fcontext_stack_t *ctx = fcontext_create(16 * 1024, my_fiber);

/* Run the fiber */
jump_fcontext(ctx->context, NULL);

/* Check stack usage */
size_t used = fcontext_get_stack_usage(ctx);
printf("Stack used: %zu bytes\n", used);

/* Destroy - automatically reports usage */
fcontext_destroy(ctx);
// Output: fcontext: stack usage: 2048 / 16384 bytes (12%)
```

**Features:**
- Detects maximum stack depth used during fiber lifetime
- Warns if usage exceeds 90%
- Zero runtime overhead (only at creation/destruction)
- Can be disabled with `#define FCONTEXT_ENABLE_STACK_WATERMARK 0`

**Design:** Metadata is allocated separately with `malloc()`, not on the stack. This prevents metadata corruption on stack overflow before the guard page is hit.

For details, see [docs/WATERMARK.md](docs/WATERMARK.md).

### High-Level Example

```c
#include <stdio.h>
#include "fcontext.h"

void fiber_func(fcontext_transfer_t t) {
    printf("Fiber running\n");
    int *counter = (int *)t.data;
    (*counter)++;
    jump_fcontext(t.prev_context, NULL);
}

int main(void) {
    printf("Page size: %zu bytes\n", fcontext_get_page_size());

    int counter = 0;

    /* Create context with automatic stack allocation and guard pages */
    fcontext_stack_t *ctx = fcontext_create(24 * 1024, fiber_func);

    /* Enter context, passing counter via data */
    fcontext_transfer_t t = jump_fcontext(ctx->context, &counter);

    printf("Counter after fiber: %d\n", counter);  /* Should be 1 */

    /* Cleanup */
    fcontext_destroy(ctx);

    return 0;
}
```

## Asymmetric Coroutines

This library implements **asymmetric coroutines** - fibers always yield to their caller, not to each other.

```c
/* Asymmetric pattern: Fibers yield only to their parent */

ev_fiber_t scheduler = ev_fiber_current();

void fiber_a(fcontext_transfer_t t) {
    printf("A1\n");
    jump_fcontext(scheduler, NULL);  /* Yield to scheduler */
    printf("A2\n");
    jump_fcontext(scheduler, NULL);  /* Yield to scheduler again */
}

int main() {
    scheduler = ev_fiber_current();

    fcontext_stack_t *a = fcontext_create(4096, fiber_a);

    /* First entry */
    fcontext_transfer_t t = jump_fcontext(a->context, NULL);  /* A1 printed */

    /* Resume */
    t = jump_fcontext(t.prev_context, NULL);  /* A2 printed */

    fcontext_destroy(a);
}
```

## Stack Size Recommendations

- **Minimum**: 4KB (one page) - for very simple functions
- **Default**: 24KB (recommended starting point)
- **Large workloads**: 64-128KB for deep call stacks

Stack size is automatically rounded up to the nearest page boundary:
```c
/* Requesting 24KB on macOS ARM64 (16KB pages) */
size_t requested = 24 * 1024;  /* 24576 bytes */
size_t actual = fcontext_align_to_page(requested);  /* 32768 bytes (2 pages) */
```

## Building

Build artifacts are placed in the `build/` directory and never clutter the source tree.

### Using CMake (Recommended)

```bash
# Clean build (recommended when switching between build systems)
rm -rf build
mkdir build
cd build

# Configure and build
cmake ..
cmake --build .

# Run tests
ctest --output-on-failure
```

**Clean builds:** If switching from Make to Ninja (or vice versa), always remove the `build/` directory first to avoid mixing build systems.

The build system automatically:
- Detects your OS (macOS, Windows, Linux) and architecture
- Selects correct assembly files for your platform
- Configures page size handling (4KB on Linux/Windows, 16KB on macOS ARM)
- Places all binaries in `build/bin/`
- Places all libraries in `build/lib/`

## Testing

Run all tests:
```bash
make test
```

Individual tests:
```bash
./test_fcontext_basic       # Low-level context switching
./test_fcontext_simple      # Simple entry point
./test_fcontext_transfer    # Data passing between contexts
```

## Implementation Details

### Guard Page Mechanism

When you call `fcontext_create()`:

1. **Determine system page size** via `sysconf(_SC_PAGE_SIZE)`
2. **Round up stack size** to nearest page boundary (resulting in N pages)
3. **Allocate address space** for N+2 pages total using `mmap(PROT_NONE)` (1 guard + N stack + 1 guard)
4. **Map stack region** (N pages) as readable/writable using `mprotect(PROT_READ|PROT_WRITE)`
5. **Leave guard pages** unmapped/protected (PROT_NONE or PAGE_GUARD)

If a fiber overflows or underflows its stack:
- Access hits protected guard page
- Kernel raises SIGSEGV (segmentation fault on POSIX) or access violation exception (Windows)
- Program terminates with clear error message

### Memory Efficiency

For a 24KB fiber on macOS ARM64 (16KB pages):
- **Virtual memory**: 64KB (4 pages worth of address space: 1 guard + 2 stack + 1 guard)
- **Physical memory**: 16KB (only one page of the stack is physically resident)
- **Address space overhead**: 32KB per fiber (2 pages for guard pages)

This is much more efficient than pre-allocating large stacks for many fibers.

## Limitations and Notes

1. **Stack grows downward** - Not suitable for systems with upward-growing stacks (uncommon)

2. **Entry function doesn't return** - Fiber function should call `jump_fcontext()` to exit:
   ```c
   void fiber_func(fcontext_transfer_t t) {
       // ... do work ...
       jump_fcontext(t.prev_context, NULL);  /* Must explicitly yield */
       /* If we reach here after being resumed, handle that */
   }
   ```

3. **Guard pages are platform-agnostic** - Uses `mmap()`/`mprotect()` on POSIX and `VirtualAlloc()`/`VirtualProtect()` on Windows

4. **Not thread-safe** - Each thread needs its own set of contexts (no shared state)

5. **Debugging** - Stack overflow in a fiber shows as segmentation fault, which is intentional

## License

Derived from Boost.Context and DaoWen/fcontext, distributed under the Boost Software License 1.0.

See `LICENSE` file for full terms.

## References

- **Boost.Context**: https://github.com/boostorg/context
- **DaoWen/fcontext**: https://github.com/DaoWen/fcontext
- **POSIX sysconf**: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sysconf.html

## Performance Notes

Context switching overhead on modern hardware (macOS M1, Intel x86-64):
- **Time per switch**: < 1 microsecond
- **Memory per fiber**: 16-24KB address space, minimal physical memory
- **Creation overhead**: < 1 microsecond

See test programs for stress-testing examples.
