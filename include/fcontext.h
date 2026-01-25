/*
 * fcontext - Stackful coroutine/fiber context switching
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * and DaoWen/fcontext (https://github.com/DaoWen/fcontext)
 *
 * Changes copyright Kyle Hayes (2026)
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 *
 * This is a low-level C11 wrapper around Boost.Context assembly code
 * providing portable stackful coroutine/fiber context switching.
 *
 * Supported Architectures and Platforms:
 * - x86_64 (Linux, macOS, Windows)
 * - ARM64/AArch64 (Linux, macOS, iOS, Android, Windows)
 * - x86/i386 (Linux, macOS)
 * - ARM (Linux, macOS, Android)
 *
 * Features:
 * - Page-aligned stack allocation (4KB on Linux/Windows, 16KB on macOS ARM)
 * - Guard pages to catch stack overflow/underflow
 * - Memory-efficient allocation (mmap on POSIX, VirtualAlloc on Windows)
 * - Cross-platform with MSVC and GCC/Clang support
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef _WIN32
    #include <sys/mman.h>
    #include <unistd.h>
#endif

#ifdef __SANITIZE_ADDRESS__
    #include <sanitizer/asan_interface.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Configuration
 * ======================================================================== */

/* Default stack size for new contexts (24KB) */
#ifndef FCONTEXT_DEFAULT_STACK_SIZE
    #define FCONTEXT_DEFAULT_STACK_SIZE (24 * 1024)
#endif

/* Enable stack watermark checking for high water mark detection */
#ifndef FCONTEXT_ENABLE_STACK_WATERMARK
    #define FCONTEXT_ENABLE_STACK_WATERMARK 1
#endif

/* Stack watermark fill pattern (0xA5 = 10100101) */
#define FCONTEXT_STACK_WATERMARK 0xA5

/* Required stack alignment (16 bytes for x86_64/ARM64 ABI compliance) */
#define FCONTEXT_STACK_ALIGNMENT 16

/* ========================================================================
 * Types
 * ======================================================================== */

/**
 * Opaque type representing a saved context/fiber.
 * Implementation details are platform-specific.
 */
typedef struct fcontext_opaque_t *fcontext_t;

/**
 * Transfer data passed between contexts on switch.
 * Contains the previous context pointer and optional user data.
 */
typedef struct {
    fcontext_t prev_context; /* Context we switched away from */
    void *data;              /* User data passed on switch */
} fcontext_transfer_t;

/**
 * Entry point function for a new context.
 * Called when context is first entered via fcontext_switch.
 * The transfer_t contains the previous context and initial data.
 */
typedef fcontext_transfer_t (*fcontext_fn_t)(fcontext_transfer_t);

/**
 * Low-level entry point function type.
 * Must return void to match the assembly implementation's ABI expectations.
 */
typedef void (*fcontext_entry_t)(fcontext_transfer_t);

/**
 * Low-Level API - Direct Assembly Interface
 * ======================================================================== */

/**
 * Create a new context pointing at the given stack and entry function.
 *
 * Parameters:
 *   sp   - Stack pointer (should point to TOP of allocated stack, not base)
 *   size - Size of stack in bytes
 *   fn   - Entry point function to call when context is first entered
 *
 * Returns:
 *   New context handle, or NULL on error
 *
 * Note: Stack grows downward. sp should be the highest address of the stack.
 * Example:
 *   char stack[24*1024];
 *   fcontext_t ctx = fcontext_init(&stack[24*1024], 24*1024, my_func);
 */
extern fcontext_t fcontext_init(void *sp, size_t size, fcontext_entry_t fn);

/**
 * Switch to a different context.
 *
 * Parameters:
 *   to - Context to switch to
 *   vp - User data pointer passed in fcontext_transfer_t.data
 *
 * Returns:
 *   Transfer structure with prev_context and data from the other context
 *
 * Note: This function does NOT return until another context switches back to us.
 * When called again on a saved context, execution resumes from where we left off.
 */
extern fcontext_transfer_t fcontext_switch(fcontext_t const to, void *vp);

/* ========================================================================
 * Page Size and Stack Alignment Utilities
 * ======================================================================== */

/**
 * Get the system page size.
 *
 * Returns:
 *   Page size in bytes (4096 on Linux/Windows, 16384 on macOS ARM, etc.)
 *   Returns FCONTEXT_DEFAULT_STACK_SIZE (24KB) if detection fails
 */
static inline size_t fcontext_get_page_size(void) {
#ifdef _WIN32
    return 4096; /* Standard page size on Windows */
#else
    long page_size = sysconf(_SC_PAGE_SIZE);
    if(page_size <= 0) { return FCONTEXT_DEFAULT_STACK_SIZE; }
    return (size_t)page_size;
#endif
}

/*
 * Align a pointer to the required stack alignment boundary (16 bytes).
 *
 * Stack pointers must be 16-byte aligned on x86_64 and ARM64 for ABI compliance.
 * Use this when allocating stacks manually with malloc().
 *
 * Parameters:
 *   ptr - Pointer to align (typically malloc result + size)
 *
 * Returns:
 *   Aligned pointer (rounded down to 16-byte boundary)
 */
static inline void *fcontext_align_stack_pointer(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void *)(addr & ~(FCONTEXT_STACK_ALIGNMENT - 1));
}

/* ========================================================================
 * High-Level Convenience API - Memory Management with Guard Pages
 * ======================================================================== */

/**
 * Stack metadata and handle.
 *
 * Stores metadata about an allocated stack including allocation method,
 * sizes, and watermark fill state. The returned pointer from stack allocation
 * functions points to this struct, from which the actual stack_top can be accessed.
 *
 * The stack_top pointer should be passed to fcontext_init() along with stack_size.
 *
 * Typical usage:
 *   fcontext_stack_t *stack = fcontext_malloc_stack(16 * 1024);
 *   fcontext_stack_t *stack = fcontext_vmem_stack(16 * 1024);
 *   fcontext_stack_fill_watermark(stack);  // Optional
 *   fcontext_t ctx = fcontext_init(stack->stack_top, stack->stack_size, my_fn);
 *   ...
 *   fcontext_stack_destroy(stack);
 */
typedef enum { FCONTEXT_ALLOC_MALLOC, FCONTEXT_ALLOC_VMEM } fcontext_alloc_type_t;

typedef struct {
    uint32_t magic;                    /* Validation (0x5A5A5A5A) */
    fcontext_alloc_type_t type;        /* MALLOC or VMEM allocation */
    size_t total_size;                 /* Total bytes allocated (for cleanup) */
    size_t stack_size;                 /* Usable stack size for fcontext_init() */
    void *stack_top;                   /* Top of usable stack, pass to fcontext_init() */
    bool watermark_enabled;            /* True if filled with 0xA5 pattern */
    void *region;                      /* Original alloc region (for mmap/VirtualAlloc cleanup) */
    fcontext_t context;                /* Initialized context for convenience */
} fcontext_stack_t;

/**
 * Allocate a stack using malloc.
 *
 * Allocates memory for a stack without filling the watermark pattern.
 * The returned stack_t contains stack_top and stack_size to pass to fcontext_init().
 *
 * Parameters:
 *   stack_size - Requested stack size in bytes.
 *                Will be rounded up to FCONTEXT_STACK_ALIGNMENT boundary.
 *
 * Returns:
 *   Stack metadata handle (containing stack_top and stack_size), or NULL on failure
 *
 * Notes:
 *   - Stack is NOT automatically filled with watermark pattern
 *   - Call fcontext_stack_fill_watermark() if you want high water mark detection
 *   - Use fcontext_stack_destroy() to clean up
 *
 * Typical usage:
 *   fcontext_stack_t *stack = fcontext_malloc_stack(16 * 1024);
 *   fcontext_stack_fill_watermark(stack);  // Optional, enables high water mark tracking
 *   fcontext_t ctx = fcontext_init(stack->stack_top, stack->stack_size, my_fn);
 *   fcontext_transfer_t t = fcontext_switch(ctx, NULL);
 *   size_t used = fcontext_max_stack_use(stack);
 *   fcontext_stack_destroy(stack);
 */
extern fcontext_stack_t *fcontext_malloc_stack(size_t stack_size);

/**
 * Allocate a stack using virtual memory (mmap/VirtualAlloc).
 *
 * Allocates virtual address space with guard pages:
 * - Bottom guard page (unmapped, will fault on underflow)
 * - Usable stack (mapped, readable/writable)
 * - Top guard page (unmapped, will fault on overflow)
 *
 * Stack is NOT automatically filled with watermark pattern.
 * The returned stack_t contains stack_top and stack_size to pass to fcontext_init().
 *
 * Parameters:
 *   stack_size - Requested stack size in bytes.
 *                Will be rounded up to nearest page boundary.
 *
 * Returns:
 *   Stack metadata handle (containing stack_top and stack_size), or NULL on failure
 *
 * Notes:
 *   - Stack is NOT automatically filled with watermark pattern
 *   - Guard pages catch overflow/underflow with segmentation fault
 *   - Filling stack with watermark pattern via fcontext_stack_fill_watermark() will
 *     cause virtual pages to get physical backing memory
 *   - Use fcontext_stack_destroy() to clean up
 *
 * Typical usage:
 *   fcontext_stack_t *stack = fcontext_vmem_stack(16 * 1024);
 *   // Don't fill watermark unless you want the memory cost
 *   fcontext_t ctx = fcontext_init(stack->stack_top, stack->stack_size, my_fn);
 *   fcontext_transfer_t t = fcontext_switch(ctx, NULL);
 *   fcontext_stack_destroy(stack);
 */
extern fcontext_stack_t *fcontext_vmem_stack(size_t stack_size);

/*
 * Fill stack memory with watermark pattern for high water mark detection.
 *
 * Fills the entire stack with 0xA5 pattern to enable detection of maximum
 * stack usage via fcontext_max_stack_use().
 *
 * Parameters:
 *   stack - Stack allocated via fcontext_malloc_stack() or fcontext_vmem_stack()
 *
 * Returns:
 *   true on success, false on error or if already filled
 *
 * Notes:
 *   - For mmap'd stacks, this causes virtual pages to get physical backing
 *   - Safe to call; watermark_enabled is checked/set to prevent double-filling
 *   - Only call this if you want to track stack usage (it has a cost for mmap stacks)
 *   - After calling, use fcontext_max_stack_use() to query the high water mark
 *
 * Typical usage:
 *   fcontext_stack_t *stack = fcontext_malloc_stack(16 * 1024);
 *   if(fcontext_stack_fill_watermark(stack)) {
 *       fcontext_t ctx = fcontext_init(stack->stack_top, stack->stack_size, my_fn);
 *       fcontext_transfer_t t = fcontext_switch(ctx, NULL);
 *       size_t used = fcontext_max_stack_use(stack);
 *       printf("Stack used: %zu / %zu bytes\n", used, stack->stack_size);
 *   }
 *   fcontext_stack_destroy(stack);
 */
extern bool fcontext_stack_fill_watermark(fcontext_stack_t *stack);

/**
 * Get the maximum stack usage (high water mark) for a stack.
 *
 * Scans the stack from bottom to top looking for the watermark pattern (0xA5).
 * Returns the number of bytes from the bottom that were overwritten.
 *
 * Parameters:
 *   stack - Stack allocated via fcontext_malloc_stack() or fcontext_vmem_stack()
 *
 * Returns:
 *   Number of bytes used from the stack bottom, or SIZE_MAX if error/not filled
 *
 * Notes:
 *   - Only accurate if fcontext_stack_fill_watermark() was called first
 *   - Returns SIZE_MAX if watermark not filled or ctx is NULL
 *   - Must be called on a terminated/paused context (not currently running)
 *
 * Typical usage:
 *   fcontext_stack_fill_watermark(stack);
 *   // ... run the context ...
 *   size_t max_used = fcontext_max_stack_use(stack);
 *   if (max_used != SIZE_MAX) {
 *       printf("Stack used: %zu bytes\n", max_used);
 *   }
 */
extern size_t fcontext_max_stack_use(fcontext_stack_t *stack);

/**
 * Check if stack was underflowed (wrote below the guard zone).
 *
 * For malloc-allocated stacks, checks if the guard zone below the stack
 * was corrupted (pattern != 0xCD).
 *
 * For mmap-allocated stacks, only detects if an actual fault occurred.
 *
 * Parameters:
 *   stack - Stack to check
 *
 * Returns:
 *   true if underflow detected, false otherwise
 *
 * Notes:
 *   - Only meaningful for malloc stacks with guard zones
 *   - mmap guard pages cause SIGSEGV, not a return code
 */
extern bool fcontext_stack_underflow(fcontext_stack_t *stack);

/**
 * Check if stack was overflowed (wrote above the guard zone).
 *
 * For malloc-allocated stacks, checks if the guard zone above the stack
 * was corrupted (pattern != 0xCD).
 *
 * For mmap-allocated stacks, only detects if an actual fault occurred.
 *
 * Parameters:
 *   stack - Stack to check
 *
 * Returns:
 *   true if overflow detected, false otherwise
 *
 * Notes:
 *   - Only meaningful for malloc stacks with guard zones
 *   - mmap guard pages cause SIGSEGV, not a return code
 */
extern bool fcontext_stack_overflow(fcontext_stack_t *stack);

/**
 * Destroy a stack allocated with fcontext_malloc_stack() or fcontext_vmem_stack().
 *
 * Frees or unmaps the allocated stack memory and the metadata structure.
 *
 * Parameters:
 *   stack - Stack to free (can be NULL)
 *
 * Notes:
 *   - Safe to call on NULL
 *   - Should not be called while contexts using the stack are running
 *   - For mmap'd stacks, unmaps the guard pages and stack region
 *   - For malloc stacks, frees the allocated memory
 */
extern void fcontext_stack_destroy(fcontext_stack_t *stack);

/**
 * Convenience wrapper around fcontext_switch with ASAN support.
 *
 * Use this when switching to contexts created with fcontext_malloc_stack()
 * or fcontext_vmem_stack().
 *
 * Parameters:
 *   ctx - Context to switch to
 *   vp  - User data pointer
 *
 * Returns:
 *   Transfer structure with previous context and data
 *
 * Typical usage:
 *   fcontext_transfer_t t = fcontext_swap(ctx->context, NULL);
 */
static inline fcontext_transfer_t fcontext_swap(fcontext_t ctx, void *vp) {
#ifdef __SANITIZE_ADDRESS__
    void *fake_stack_save = NULL;
    __sanitizer_start_switch_fiber(&fake_stack_save, NULL, 0);
#endif
    fcontext_transfer_t t = fcontext_switch(ctx, vp);
#ifdef __SANITIZE_ADDRESS__
    __sanitizer_finish_switch_fiber(fake_stack_save, NULL, NULL);
#endif
    return t;
}

#ifdef __cplusplus
}
#endif

