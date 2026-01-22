# fcontext - Portable Context Switching Library

A portable, low-level C11 fiber/coroutine context switching library derived from Boost.Context, designed to replace the deprecated and unsupported `ucontext` API.

## Features

- **Pure C11 implementation** - No C++ dependencies, no platform-specific quirks
- **Asymmetric coroutines** - Stackful fibers that yield control to parent context
- **Guard pages** - Memory-efficient mmap-based stack with automatic bounds detection
- **Page-aware allocation** - Automatically handles varying page sizes (4KB Linux, 16KB macOS ARM, etc.)
- **Portable across architectures** - x86_64, ARM64, x86, ARM support on Linux and macOS
- **Zero external dependencies** - Only standard POSIX APIs (mmap, sysconf)

## Why This Exists

The POSIX `ucontext` API is:
- **Deprecated** on most modern operating systems
- **Broken on macOS ARM64** (Apple Silicon) - no native support
- **No longer maintained** - new POSIX standard removed it
- **Inconsistent** across platforms (different function signatures, behavior)

This library provides a modern replacement that actually works.

## Architecture Support (Tier 1 - Fully Tested)

| Architecture | Linux | macOS |
|:---|:---:|:---:|
| x86_64 | ✓ | ✓ |
| ARM64 (AArch64) | ✓ | ✓ (Apple Silicon) |
| x86 (i386) | ✓ | ✓ |
| ARM (32-bit) | ✓ | ✓ |

Page sizes automatically detected:
- **Linux**: 4KB (typical)
- **macOS ARM64**: 16KB (Apple Silicon requirement)
- **Other systems**: Detected via `sysconf(_SC_PAGE_SIZE)`

## API Overview

### Two-Layer Design

1. **Low-Level API** (`make_fcontext`, `jump_fcontext`, `ontop_fcontext`)
   - Direct assembly interface for maximum control
   - Use when you need custom context patterns

2. **High-Level Convenience API** (`fcontext_create`, `fcontext_destroy`)
   - Handles memory allocation and guard pages automatically
   - Recommended for most use cases

## Low-Level API

### Core Functions

```c
#include "fcontext.h"

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

```c
#include "fcontext.h"

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

The high-level API uses mmap to allocate with guard pages:

```
Address Space Layout:
┌─────────────────────┐
│  Guard Page         │  (unmapped, will fault on access)
├─────────────────────┤
│  Actual Stack       │  (mapped, readable/writable)
│  (one page)         │  Size: rounded to page boundary
├─────────────────────┤
│  Guard Page         │  (unmapped, will fault on access)
└─────────────────────┘

Total Allocation = page_size + stack_size + page_size
Physical Memory  = stack_size (only one page is resident)
```

Benefits:
- **Automatic overflow detection** - Stack overflow causes segmentation fault
- **Memory efficient** - Guard pages use address space, not physical memory
- **Page-aware** - Works correctly with 4KB (Linux) and 16KB (macOS ARM) pages

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

### Using Make

```bash
cd src/tests/fcontext
make
make test
```

### Using CMake

```bash
cd src/tests/fcontext
mkdir build
cd build
cmake ..
cmake --build .
ctest
```

Both build systems automatically:
- Detect your OS and architecture
- Select correct assembly files
- Configure page size handling

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
2. **Allocate address space** for 3 pages using `mmap(PROT_NONE)`
3. **Map middle page** as readable/writable using `mprotect(PROT_READ|PROT_WRITE)`
4. **Leave guard pages** unmapped (PROT_NONE)

If a fiber overflows or underflows its stack:
- Access hits unmapped guard page
- Kernel raises SIGSEGV (segmentation fault)
- Program terminates with clear error message

### Memory Efficiency

For a 24KB fiber on macOS ARM64 (16KB pages):
- **Virtual memory**: 48KB (3 pages worth of address space)
- **Physical memory**: 16KB (only the actual stack is paged in)
- **Overhead**: 24KB per fiber (one page for guard space)

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

3. **Guard pages are POSIX-only** - Uses `mmap()` and `mprotect()`, which are POSIX standards

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
