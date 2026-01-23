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
#include <string.h>
#include <stdio.h>
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

    /* Allocate metadata structure SEPARATELY from stack to prevent corruption on overflow */
    fcontext_stack_t *state = (fcontext_stack_t *)malloc(sizeof(fcontext_stack_t));
    if (state == NULL) {
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, total_size);
#endif
        return NULL;
    }

#if FCONTEXT_ENABLE_STACK_WATERMARK
    /* Fill the ENTIRE stack with watermark pattern for high water mark detection */
    /* No metadata stored on stack, so we can fill it all */
    memset(stack_addr, FCONTEXT_STACK_WATERMARK, stack_size);
#endif

    /* Initialize context at top of stack (stacks grow downward) */
    /* Note: Stack pointer is automatically aligned to 16 bytes by make_fcontext assembly */
    void *stack_top = (char *)stack_addr + stack_size;
    /* Ensure 16-byte alignment for ABI compliance */
    stack_top = fcontext_align_stack_pointer(stack_top);
    fcontext_t ctx = make_fcontext(stack_top, stack_size, entry_fn);
    if (ctx == NULL) {
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, total_size);
#endif
        free(state);
        return NULL;
    }

    /* Store allocation info for cleanup */
    state->context = ctx;
    state->mmap_base = region;
    state->stack_base = stack_addr;
    state->mmap_size = total_size;
    state->stack_size = stack_size;

    return state;
}

/**
 * Get the stack usage (high water mark) for a context.
 *
 * Scans the stack from bottom to top looking for the watermark pattern.
 * Returns the maximum stack depth used.
 */
size_t fcontext_get_stack_usage(const fcontext_stack_t *ctx) {
    if (ctx == NULL || ctx->stack_size == 0 || ctx->stack_base == NULL) {
        return 0;
    }

#if FCONTEXT_ENABLE_STACK_WATERMARK
    /* Stack base pointer points to the actual stack (after bottom guard page) */
    const uint8_t *stack_bottom = (const uint8_t *)ctx->stack_base;

    /* Scan from bottom to top to find where watermark pattern ends */
    /* No metadata on stack, so we scan the entire stack region */
    size_t unused_bytes = 0;
    for (size_t i = 0; i < ctx->stack_size; i++) {
        if (stack_bottom[i] != FCONTEXT_STACK_WATERMARK) {
            /* Found modified stack memory - this is where stack usage started */
            break;
        }
        unused_bytes++;
    }

    /* Return bytes used (total - unused) */
    size_t used_bytes = ctx->stack_size - unused_bytes;
    return used_bytes;
#else
    return 0;  /* Watermark checking disabled */
#endif
}

/**
 * Destroy a context created with fcontext_create().
 * Frees the guarded stack region and metadata (platform-agnostic).
 */
void fcontext_destroy(fcontext_stack_t *ctx) {
    if (ctx == NULL) {
        return;
    }

#if FCONTEXT_ENABLE_STACK_WATERMARK
    /* Check and report stack usage before destroying */
    size_t used = fcontext_get_stack_usage(ctx);
    if (used > 0) {
        size_t percent = (used * 100) / ctx->stack_size;
        fprintf(stderr, "fcontext: stack usage: %zu / %zu bytes (%zu%%)\n",
                used, ctx->stack_size, percent);

        /* Warn if stack usage is high */
        if (percent > 90) {
            fprintf(stderr, "fcontext: WARNING: stack usage exceeded 90%% - consider increasing stack size\n");
        }
    }
#endif

    /* Free the stack region */
    if (ctx->mmap_base != NULL && ctx->mmap_size > 0) {
#ifdef _WIN32
        VirtualFree(ctx->mmap_base, 0, MEM_RELEASE);
#else
        munmap(ctx->mmap_base, ctx->mmap_size);
#endif
    }

    /* Free the metadata structure (allocated separately with malloc) */
    free(ctx);
}
