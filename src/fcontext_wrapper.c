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
 * Create a new context using malloc with software guard zones.
 */
fcontext_stack_t *fcontext_create_malloc(size_t stack_size, size_t guard_size, fcontext_fn_t entry_fn) {
    fcontext_stack_t *ctx = (fcontext_stack_t *)malloc(sizeof(fcontext_stack_t));
    if (!ctx) return NULL;

    if (!entry_fn) { free(ctx); return NULL; }
    if (stack_size == 0) stack_size = FCONTEXT_DEFAULT_STACK_SIZE;

    /* Align both stack and guard sizes to ensure offsets maintain alignment */
    stack_size = (stack_size + FCONTEXT_STACK_ALIGNMENT - 1) & ~(FCONTEXT_STACK_ALIGNMENT - 1);
    guard_size = (guard_size + FCONTEXT_STACK_ALIGNMENT - 1) & ~(FCONTEXT_STACK_ALIGNMENT - 1);

    size_t inner_size = guard_size + stack_size + guard_size;
    /* Over-allocate to guarantee we can find an aligned spot for the block */
    size_t total_alloc_size = inner_size + FCONTEXT_STACK_ALIGNMENT;

    void *raw_alloc = malloc(total_alloc_size);
    if (!raw_alloc) { free(ctx); return NULL; }

    /* Find an aligned address inside the allocation for our [guard][stack][guard] block */
    uintptr_t aligned_addr = ((uintptr_t)raw_alloc + FCONTEXT_STACK_ALIGNMENT - 1) & ~(FCONTEXT_STACK_ALIGNMENT - 1);
    void *aligned_base = (void*)aligned_addr;

    void *stack_base = (char *)aligned_base + guard_size;
    void *sp = (char *)stack_base + stack_size;

    /* Setup guard zones (canaries) */
    if (guard_size > 0) {
        memset(aligned_base, 0xCD, guard_size);
        memset(sp, 0xCD, guard_size);
    }

    /* Setup watermark */
#if FCONTEXT_ENABLE_STACK_WATERMARK
    memset(stack_base, FCONTEXT_STACK_WATERMARK, stack_size);
#endif

    /* The stack pointer passed to make_fcontext must be aligned */
    sp = fcontext_align_stack_pointer(sp);
    
    ctx->context = make_fcontext(sp, stack_size, entry_fn);
    ctx->alloc_type = FCONTEXT_ALLOC_MALLOC;
    ctx->alloc_base = raw_alloc; /* Store original malloc pointer for free() */
    ctx->stack_base = stack_base;
    ctx->alloc_size = total_alloc_size;
    ctx->stack_size = stack_size;
    ctx->guard_size = guard_size;

    return ctx;
}

/**
 * Create a new context using mmap/VirtualAlloc with hardware guard pages.
 */
fcontext_stack_t *fcontext_create_mmap(size_t stack_size, size_t guard_size, fcontext_fn_t entry_fn) {
    if (!entry_fn) return NULL;

    size_t page_size = get_page_size();
    if (stack_size == 0) stack_size = FCONTEXT_DEFAULT_STACK_SIZE;

    /* Align sizes to page boundary */
    stack_size = ((stack_size + page_size - 1) / page_size) * page_size;
    guard_size = ((guard_size + page_size - 1) / page_size) * page_size;

    size_t total_size = guard_size + stack_size + guard_size;

    void *region = NULL;
    void *stack_base = NULL;

#ifdef _WIN32
    region = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!region) return NULL;

    if (guard_size > 0) {
        DWORD old;
        if (!VirtualProtect(region, guard_size, PAGE_GUARD | PAGE_READWRITE, &old)) {
            VirtualFree(region, 0, MEM_RELEASE);
            return NULL;
        }
        if (!VirtualProtect((char *)region + guard_size + stack_size, guard_size, PAGE_GUARD | PAGE_READWRITE, &old)) {
            VirtualFree(region, 0, MEM_RELEASE);
            return NULL;
        }
    }
    stack_base = (char *)region + guard_size;
#else
    region = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) return NULL;

    if (guard_size > 0) {
        if (mprotect(region, guard_size, PROT_NONE) == -1) {
            munmap(region, total_size);
            return NULL;
        }
        if (mprotect((char *)region + guard_size + stack_size, guard_size, PROT_NONE) == -1) {
            munmap(region, total_size);
            return NULL;
        }
    }
    stack_base = (char *)region + guard_size;
#endif

    fcontext_stack_t *ctx = (fcontext_stack_t *)malloc(sizeof(fcontext_stack_t));
    if (!ctx) {
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, total_size);
#endif
        return NULL;
    }

#if FCONTEXT_ENABLE_STACK_WATERMARK
    memset(stack_base, FCONTEXT_STACK_WATERMARK, stack_size);
#endif

    void *sp = (char *)stack_base + stack_size;
    ctx->context = make_fcontext(sp, stack_size, entry_fn);
    ctx->alloc_type = FCONTEXT_ALLOC_MMAP;
    ctx->alloc_base = region;
    ctx->stack_base = stack_base;
    ctx->alloc_size = total_size;
    ctx->stack_size = stack_size;
    ctx->guard_size = guard_size;

    return ctx;
}

fcontext_stack_t *fcontext_create(size_t stack_size, fcontext_fn_t entry_fn) {
    /* Default to mmap with 1 page guard size */
    return fcontext_create_mmap(stack_size, get_page_size(), entry_fn);
}

void fcontext_destroy(fcontext_stack_t *ctx) {
    if (!ctx) return;

#ifndef NDEBUG /* In Debug builds, NDEBUG is not defined */
#if FCONTEXT_ENABLE_STACK_WATERMARK
    if (ctx->stack_size > 0) {
        size_t used = fcontext_get_stack_usage(ctx);
        double usage_percent = (double)used / ctx->stack_size * 100.0;
        fprintf(stderr, "fcontext_destroy [Debug]: stack usage: %zu / %zu bytes (%.1f%%)\n",
                used, ctx->stack_size, usage_percent);
        if (usage_percent > 90.0) {
            fprintf(stderr, "fcontext_destroy [Debug]: WARNING: stack usage is over 90%%!\n");
        }
    }
#endif
#endif

    if (ctx->alloc_type == FCONTEXT_ALLOC_MMAP) {
#ifdef _WIN32
        VirtualFree(ctx->alloc_base, 0, MEM_RELEASE);
#else
        munmap(ctx->alloc_base, ctx->alloc_size);
#endif
    } else {
        free(ctx->alloc_base);
    }
    free(ctx);
}

size_t fcontext_get_stack_usage(const fcontext_stack_t *ctx) {
    if (!ctx || !ctx->stack_base || ctx->stack_size == 0) return 0;

#if FCONTEXT_ENABLE_STACK_WATERMARK
    unsigned char *ptr = (unsigned char *)ctx->stack_base;
    size_t unused = 0;
    while (unused < ctx->stack_size && ptr[unused] == FCONTEXT_STACK_WATERMARK) {
        unused++;
    }
    return ctx->stack_size - unused;
#else
    return 0;
#endif
}