/**
 * fcontext - Stackful coroutine/fiber context switching
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * and DaoWen/fcontext (https://github.com/DaoWen/fcontext)
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

#ifndef FCONTEXT_H_
#define FCONTEXT_H_

#include <stddef.h>
#include <stdint.h>
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
 * Called when context is first entered via jump_fcontext.
 * The transfer_t contains the previous context and initial data.
 */
typedef void (*fcontext_fn_t)(fcontext_transfer_t);

/**
 * Function run "on top" of an existing context.
 * Used with ontop_fcontext to modify behavior of a context.
 */
typedef fcontext_transfer_t (*fcontext_ontop_fn_t)(fcontext_transfer_t);

/* ========================================================================
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
 *   fcontext_t ctx = make_fcontext(&stack[24*1024], 24*1024, my_func);
 */
extern fcontext_t make_fcontext(void *sp, size_t size, fcontext_fn_t fn);

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
extern fcontext_transfer_t jump_fcontext(fcontext_t const to, void *vp);

/**
 * Switch to a context and run a function on top of it.
 *
 * Parameters:
 *   to - Context to switch to
 *   vp - User data pointer passed to ontop function
 *   fn - Function to call "on top" of the context
 *
 * Returns:
 *   Transfer structure returned by fn
 *
 * Advanced feature: fn will be called with the context transfer and can
 * modify the return value before control passes back.
 */
extern fcontext_transfer_t ontop_fcontext(fcontext_t const to, void *vp, fcontext_ontop_fn_t fn);

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

/**
 * Round size up to nearest multiple of page size.
 *
 * Parameters:
 *   size - Size to align
 *
 * Returns:
 *   Size rounded up to next page boundary
 */
static inline size_t fcontext_align_to_page(size_t size) {
    size_t page_size = fcontext_get_page_size();
    return ((size + page_size - 1) / page_size) * page_size;
}

/**
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
 * Context with allocated stack and guard pages.
 *
 * Metadata structure is allocated separately from the stack to prevent
 * corruption on stack overflow. The stack region contains only:
 * - Guard page at bottom (unmapped)
 * - Actual stack in middle (mapped, readable/writable, filled with watermark)
 * - Guard page at top (unmapped)
 *
 * This catches stack overflow/underflow while using minimal memory.
 */
typedef enum { FCONTEXT_ALLOC_MMAP, FCONTEXT_ALLOC_MALLOC } fcontext_alloc_type_t;

typedef struct {
    fcontext_t context;
    fcontext_alloc_type_t alloc_type;
    void *alloc_base;  /* Base of allocated region (mmap or malloc) */
    void *stack_base;  /* Base of actual stack (after bottom guard page) */
    size_t alloc_size; /* Total size of allocated region */
    size_t stack_size; /* Size of actual stack (excludes guard pages) */
    size_t guard_size; /* Size of guard pages/zones */
} fcontext_stack_t;

/**
 * Create a new context with guarded stack using mmap.
 *
 * Allocates address space for (page_size + stack_size + page_size):
 * - Bottom guard page (unmapped, will fault on overflow)
 * - Stack page (mapped and accessible)
 * - Top guard page (unmapped, will fault on underflow)
 *
 * Parameters:
 *   stack_size - Requested stack size in bytes (default: FCONTEXT_DEFAULT_STACK_SIZE)
 *                Will be rounded up to nearest page boundary
 *   entry_fn   - Entry point function
 *
 * Returns:
 *   Context with allocated and guarded stack, or NULL on failure
 *
 * Notes:
 *   - Stack is page-aligned for system page size (4KB on Linux, 16KB on macOS ARM)
 *   - Uses mmap for efficient guard page implementation
 *   - Guard pages catch overflow/underflow with segmentation fault
 *   - To catch this signal, use sigaltstack() and SA_ONSTACK, as the fiber stack will be invalid
 *   - Use fcontext_destroy() to clean up
 */
extern fcontext_stack_t *fcontext_create(size_t stack_size, fcontext_fn_t entry_fn);

/**
 * Create a new context using malloc with software guard zones.
 *
 * Allocates: [Guard] [Stack] [Guard]
 * Guards are filled with 0xCD pattern.
 * Stack is filled with watermark pattern.
 *
 * Parameters:
 *   stack_size - Size of stack
 *   guard_size - Size of guard zones (canaries)
 *   entry_fn   - Entry point
 */
extern fcontext_stack_t *fcontext_create_malloc(size_t stack_size, size_t guard_size, fcontext_fn_t entry_fn);

/**
 * Create a new context using mmap/VirtualAlloc with hardware guard pages.
 *
 * Parameters:
 *   stack_size - Size of stack
 *   guard_size - Size of guard pages (rounded up to page size)
 *                Both stack_size and guard_size will be rounded up to the
 *                nearest system page size.
 *   entry_fn   - Entry point
 */
extern fcontext_stack_t *fcontext_create_mmap(size_t stack_size, size_t guard_size, fcontext_fn_t entry_fn);

/**
 * Destroy a context created with fcontext_create().
 *
 * Parameters:
 *   ctx - Context to free (can be NULL)
 *
 * Notes:
 *   - Unmaps the guarded stack region
 *   - Safe to call on NULL
 */
extern void fcontext_destroy(fcontext_stack_t *ctx);

/**
 * Get the stack usage (high water mark) for a context.
 *
 * Scans the stack from bottom to top looking for the watermark pattern (0xA5).
 * Returns the maximum stack depth used by the coroutine.
 *
 * Parameters:
 *   ctx - Context to check
 *
 * Returns:
 *   Number of bytes used from the stack (0 if watermark checking is disabled)
 *
 * Notes:
 *   - Only works if FCONTEXT_ENABLE_STACK_WATERMARK is enabled
 *   - Stack must have been filled with watermark pattern at creation
 *   - Returns 0 if ctx is NULL or watermark checking is disabled
 */
extern size_t fcontext_get_stack_usage(const fcontext_stack_t *ctx);

/**
 * Switch to a context created with fcontext_create().
 * Convenience wrapper around jump_fcontext.
 *
 * Parameters:
 *   ctx - Context to switch to
 *   vp  - User data pointer
 *
 * Returns:
 *   Transfer structure with previous context and data
 */
static inline fcontext_transfer_t fcontext_swap(fcontext_t ctx, void *vp) {
#ifdef __SANITIZE_ADDRESS__
    void *fake_stack_save = NULL;
    __sanitizer_start_switch_fiber(&fake_stack_save, NULL, 0);
#endif
    fcontext_transfer_t t = jump_fcontext(ctx, vp);
#ifdef __SANITIZE_ADDRESS__
    __sanitizer_finish_switch_fiber(fake_stack_save, NULL, NULL);
#endif
    return t;
}

#ifdef __cplusplus
}
#endif

#endif /* FCONTEXT_H_ */
