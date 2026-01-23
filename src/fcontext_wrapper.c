/**
 * fcontext_wrapper.c
 * High-level convenience API for fcontext
 *
 * Implements page-aligned stack allocation with guard pages.
 * Uses mmap/mprotect on POSIX systems and VirtualAlloc/VirtualProtect on Windows.
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include <stdlib.h>
#include <errno.h>
#include "fcontext.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

/* ========================================================================
 * Stack Allocation with Guard Pages
 * ======================================================================== */

/**
 * Get system page size (platform-agnostic).
 */
static size_t get_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 0) {
        return 4096;  /* Default to 4KB if sysconf fails */
    }
    return (size_t)page_size;
#endif
}

/**
 * Create a new context with guarded stack.
 *
 * Allocates stack with guard pages on both ends:
 *   [Guard Page] [Stack] [Guard Page]
 *   unmapped     R/W     unmapped
 *
 * On POSIX systems, uses mmap/mprotect.
 * On Windows, uses VirtualAlloc/VirtualProtect.
 * This catches overflow/underflow without using extra physical memory.
 */
fcontext_stack_t *fcontext_create(size_t stack_size, fcontext_fn_t entry_fn) {
    if (entry_fn == NULL) {
        return NULL;
    }

    size_t page_size = get_page_size();

    /* Determine stack size */
    if (stack_size == 0) {
        stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
    }

    /* Align to page size */
    stack_size = ((stack_size + page_size - 1) / page_size) * page_size;

    /* Total allocation: guard_page + stack + guard_page */
    size_t total_size = page_size + stack_size + page_size;

    void *region = NULL;
    void *stack_addr = NULL;

#ifdef _WIN32
    /* Windows implementation using VirtualAlloc */
    region = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (region == NULL) {
        return NULL;
    }

    /* Set guard page at bottom (start of allocation) */
    DWORD old_protect;
    if (!VirtualProtect(region, page_size, PAGE_GUARD | PAGE_READWRITE, &old_protect)) {
        VirtualFree(region, 0, MEM_RELEASE);
        return NULL;
    }

    /* Stack is in the middle of the allocation */
    stack_addr = (char *)region + page_size;

    /* Set guard page at top (end of stack) */
    if (!VirtualProtect((char *)region + page_size + stack_size, page_size,
                        PAGE_GUARD | PAGE_READWRITE, &old_protect)) {
        VirtualFree(region, 0, MEM_RELEASE);
        return NULL;
    }
#else
    /* POSIX implementation using mmap/mprotect */
    region = mmap(NULL, total_size, PROT_NONE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        return NULL;
    }

    /* Map the middle section (stack) as readable and writable */
    stack_addr = (char *)region + page_size;
    if (mprotect(stack_addr, stack_size, PROT_READ | PROT_WRITE) != 0) {
        munmap(region, total_size);
        return NULL;
    }
#endif

    /* Create metadata structure at the beginning of the stack */
    fcontext_stack_t *state = (fcontext_stack_t *)stack_addr;

    /* Initialize context at top of stack (stacks grow downward) */
    void *stack_top = (char *)stack_addr + stack_size;
    fcontext_t ctx = make_fcontext(stack_top, stack_size, entry_fn);
    if (ctx == NULL) {
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, total_size);
#endif
        return NULL;
    }

    /* Store allocation info for cleanup */
    state->context = ctx;
    state->mmap_base = region;
    state->mmap_size = total_size;
    state->stack_size = stack_size;

    return state;
}

/**
 * Destroy a context created with fcontext_create().
 * Frees the guarded stack region (platform-agnostic).
 */
void fcontext_destroy(fcontext_stack_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->mmap_base != NULL && ctx->mmap_size > 0) {
#ifdef _WIN32
        VirtualFree(ctx->mmap_base, 0, MEM_RELEASE);
#else
        munmap(ctx->mmap_base, ctx->mmap_size);
#endif
    }
}
