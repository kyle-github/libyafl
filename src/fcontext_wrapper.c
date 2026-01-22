/**
 * fcontext_wrapper.c
 * High-level convenience API for fcontext
 *
 * Implements page-aligned stack allocation with guard pages using mmap.
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include "fcontext.h"

/* ========================================================================
 * Stack Allocation with Guard Pages
 * ======================================================================== */

/**
 * Create a new context with guarded stack using mmap.
 *
 * Layout (3 pages total):
 *   [Guard Page] [Stack Page] [Guard Page]
 *   unmapped     mapped R/W   unmapped
 *
 * This catches overflow/underflow without using extra physical memory.
 */
fcontext_stack_t *fcontext_create(size_t stack_size, fcontext_fn_t entry_fn) {
    if (entry_fn == NULL) {
        return NULL;
    }

    size_t page_size = fcontext_get_page_size();

    /* Determine stack size */
    if (stack_size == 0) {
        stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
    }

    /* Align to page size */
    stack_size = fcontext_align_to_page(stack_size);

    /* Total allocation: guard_page + stack + guard_page */
    size_t total_size = page_size + stack_size + page_size;

    /* Allocate address space (PROT_NONE means not yet mapped) */
    void *region = mmap(NULL, total_size, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        return NULL;
    }

    /* Map the middle section (stack) as readable and writable */
    void *stack_addr = (char *)region + page_size;
    if (mprotect(stack_addr, stack_size, PROT_READ | PROT_WRITE) != 0) {
        munmap(region, total_size);
        return NULL;
    }

    /* Create metadata structure at the beginning of the stack */
    fcontext_stack_t *state = (fcontext_stack_t *)stack_addr;

    /* Initialize context at top of stack (stacks grow downward) */
    void *stack_top = (char *)stack_addr + stack_size;
    fcontext_t ctx = make_fcontext(stack_top, stack_size, entry_fn);
    if (ctx == NULL) {
        munmap(region, total_size);
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
 * Unmaps the guarded stack region.
 */
void fcontext_destroy(fcontext_stack_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->mmap_base != NULL && ctx->mmap_size > 0) {
        munmap(ctx->mmap_base, ctx->mmap_size);
    }
}
