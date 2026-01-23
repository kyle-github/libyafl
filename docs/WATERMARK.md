# Stack Watermark Checking and Alignment

## Overview

The fcontext library now includes stack watermark checking for detecting high water marks and ensuring proper 16-byte stack alignment for ABI compliance.

## Features

### 1. Stack Watermark Checking

When enabled, the entire stack is filled with a watermark pattern (`0xA5`) at creation. When the context is destroyed, the library scans from the bottom of the stack upward to find where the watermark was overwritten, revealing the maximum stack depth used.

**Configuration:**
```c
/* In fcontext.h - enabled by default */
#ifndef FCONTEXT_ENABLE_STACK_WATERMARK
#define FCONTEXT_ENABLE_STACK_WATERMARK 1
#endif

#define FCONTEXT_STACK_WATERMARK 0xA5
```

**API:**
```c
/* Get stack usage in bytes */
size_t fcontext_get_stack_usage(const fcontext_stack_t *ctx);
```

**Automatic Reporting:**
When `fcontext_destroy()` is called, it automatically prints stack usage:
```
fcontext: stack usage: 1248 / 16384 bytes (7%)
```

If usage exceeds 90%, a warning is issued:
```
fcontext: WARNING: stack usage exceeded 90% - consider increasing stack size
```

### 2. 16-Byte Stack Alignment

All modern x86_64 and ARM64 ABIs require 16-byte stack alignment. The library provides a helper for manual stack allocation:

```c
/* Align a pointer to 16-byte boundary (rounds down) */
void* fcontext_align_stack_pointer(void* ptr);
```

**Example with malloc:**
```c
size_t stack_size = 8 * 1024;
void *stack = malloc(stack_size);

/* Calculate aligned stack top */
void *stack_top = (char *)stack + stack_size;
void *aligned_top = fcontext_align_stack_pointer(stack_top);

/* Create context with aligned stack */
fcontext_t ctx = make_fcontext(aligned_top, stack_size, my_function);
```

**Note:** `fcontext_create()` automatically applies 16-byte alignment - you only need the helper for manual `malloc()` stacks.

## Design Decisions

### Metadata Stored Separately

The metadata structure (`fcontext_stack_t`) is **allocated separately with `malloc()`**, not on the stack itself. This prevents corruption on stack overflow:

```
[Guard Page]
[Stack - filled with 0xA5 watermark]  ← No metadata here!
[Guard Page]

Metadata allocated separately with malloc()
```

If metadata were stored at the bottom of the stack, an overflow would corrupt it before hitting the guard page, defeating the entire purpose of guard pages.

### Stack Layout

```
Total allocation via mmap/VirtualAlloc:
┌─────────────────────┐
│  Guard Page         │ ← PAGE_NOACCESS / PROT_NONE
├─────────────────────┤
│  Actual Stack       │ ← Filled with 0xA5
│  (grows downward)   │ ← All available for stack use
├─────────────────────┤
│  Guard Page         │ ← PAGE_NOACCESS / PROT_NONE
└─────────────────────┘

Metadata (malloc'd separately):
struct fcontext_stack_t {
    fcontext_t context;
    void *mmap_base;     // For cleanup
    void *stack_base;    // Points to stack (after guard)
    size_t mmap_size;
    size_t stack_size;
}
```

## Performance Impact

- **Watermark filling:** One `memset()` at context creation (negligible)
- **Watermark scanning:** One linear scan at context destruction (< 1μs for typical stacks)
- **Memory overhead:** `sizeof(fcontext_stack_t)` = ~48 bytes per context (malloc'd separately)
- **Runtime overhead:** None during context switches

## Disabling Watermark Checking

To disable watermark checking (saves one `memset()` and reporting overhead):

```c
/* Define before including fcontext.h */
#define FCONTEXT_ENABLE_STACK_WATERMARK 0
#include "fcontext.h"
```

When disabled:
- No watermark pattern is written
- `fcontext_get_stack_usage()` returns 0
- No usage reporting at destruction

## Example Usage

```c
#include <stdio.h>
#include "fcontext.h"

void my_fiber(fcontext_transfer_t t) {
    char buffer[2048];  // Consume some stack
    sprintf(buffer, "Using stack...");

    jump_fcontext(t.prev_context, NULL);
}

int main(void) {
    /* Create context with watermark checking */
    fcontext_stack_t *ctx = fcontext_create(16 * 1024, my_fiber);

    /* Run the fiber */
    jump_fcontext(ctx->context, NULL);

    /* Check usage before destroying */
    size_t used = fcontext_get_stack_usage(ctx);
    printf("Stack used: %zu bytes\n", used);

    /* Destroy - will also print usage */
    fcontext_destroy(ctx);

    return 0;
}
```

Output:
```
Stack used: 2304 bytes
fcontext: stack usage: 2304 / 16384 bytes (14%)
```

## Testing

Run the watermark test:
```bash
./bin/test_fcontext_watermark
```

This tests:
- Stack alignment helper
- Small vs large stack usage detection
- Manual malloc stack alignment
- Automatic reporting

## Platform Support

- **Linux**: Full support (mmap + watermark)
- **macOS**: Full support (mmap + watermark)
- **Windows**: Full support (VirtualAlloc + watermark)

All platforms support the same API and produce identical behavior.
