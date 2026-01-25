/**
 * fcontext_wrapper.c
 * High-level convenience API for fcontext
 *
 * Implements stack allocation with optional guard pages and watermark support.
 * Uses malloc on all platforms for malloc_stack.
 * Uses mmap/VirtualAlloc on POSIX/Windows for vmem_stack with hardware guard pages.
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "fcontext.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

/* Metadata magic number for validation */
#define FCONTEXT_STACK_MAGIC 0x5A5A5A5A

/* ========================================================================
 * Stack Allocation Utilities
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
    if(page_size <= 0) { return 4096; /* Default to 4KB if sysconf fails */ }
    return (size_t)page_size;
#endif
}

/* ========================================================================
 * malloc-based Stack Allocation
 * ======================================================================== */

/**
 * Allocate a stack using malloc.
 *
 * The returned stack metadata contains:
 * - stack_top: pointer to top of allocated stack
 * - stack_size: usable stack size
 * - type: FCONTEXT_ALLOC_MALLOC
 * - watermark_enabled: false (caller can fill if desired)
 *
 * Stack is allocated as one contiguous block and NOT filled with watermark.
 */
fcontext_stack_t *fcontext_malloc_stack(size_t stack_size) {
    if(stack_size == 0) {
        stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
    }

    /* Align stack size to FCONTEXT_STACK_ALIGNMENT boundary */
    stack_size = (stack_size + FCONTEXT_STACK_ALIGNMENT - 1) & ~(FCONTEXT_STACK_ALIGNMENT - 1);

    /* Allocate: [metadata][stack] */
    fcontext_stack_t *meta = (fcontext_stack_t *)malloc(sizeof(fcontext_stack_t) + stack_size);
    if(!meta) {
        return NULL;
    }

    /* Stack grows downward, so stack_top is at the end of the allocation */
    void *stack_memory = (char *)meta + sizeof(fcontext_stack_t);
    void *stack_top = (char *)stack_memory + stack_size;

    /* Align stack_top to FCONTEXT_STACK_ALIGNMENT boundary (round down) */
    stack_top = fcontext_align_stack_pointer(stack_top);

    /* Initialize metadata */
    meta->magic = FCONTEXT_STACK_MAGIC;
    meta->type = FCONTEXT_ALLOC_MALLOC;
    meta->total_size = sizeof(fcontext_stack_t) + stack_size;
    meta->stack_size = stack_size;
    meta->stack_top = stack_top;
    meta->watermark_enabled = false;
    meta->region = (void *)meta;  /* For MALLOC, region points to metadata itself */

    return meta;
}

/* ========================================================================
 * mmap/VirtualAlloc-based Stack Allocation with Guard Pages
 * ======================================================================== */

/**
 * Allocate a stack using virtual memory (mmap on POSIX, VirtualAlloc on Windows).
 *
 * Allocates: [guard page(s)] [stack] [guard page(s)]
 *
 * Guard pages are not filled with any pattern initially - they are simply
 * marked as inaccessible. Accessing a guard page will cause a fault.
 *
 * The returned stack metadata contains:
 * - stack_top: pointer to top of usable stack (inside protected region)
 * - stack_size: size of usable stack
 * - type: FCONTEXT_ALLOC_VMEM
 * - watermark_enabled: false (caller can fill if desired)
 *
 * Stack is NOT filled with watermark initially. Caller can optionally call
 * fcontext_stack_fill_watermark() to enable high water mark detection.
 */
fcontext_stack_t *fcontext_vmem_stack(size_t stack_size) {
    size_t page_size = get_page_size();
    if(stack_size == 0) {
        stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
    }

    /* Round stack size up to page boundary */
    size_t aligned_stack_size = ((stack_size + page_size - 1) / page_size) * page_size;

    /* One guard page on each side */
    size_t guard_size = page_size;
    size_t total_size = guard_size + aligned_stack_size + guard_size;

    void *region = NULL;
    void *stack_base = NULL;

#ifdef _WIN32
    /* Windows: VirtualAlloc for reserve+commit, VirtualProtect for guard pages */
    region = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if(!region) {
        return NULL;
    }

    /* Protect bottom guard page */
    DWORD old;
    if(!VirtualProtect(region, guard_size, PAGE_NOACCESS, &old)) {
        VirtualFree(region, 0, MEM_RELEASE);
        return NULL;
    }

    /* Protect top guard page */
    if(!VirtualProtect((char *)region + guard_size + aligned_stack_size, guard_size, PAGE_NOACCESS, &old)) {
        VirtualFree(region, 0, MEM_RELEASE);
        return NULL;
    }

    stack_base = (char *)region + guard_size;
#else
    /* POSIX: mmap then mprotect for guard pages */
    region = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(region == MAP_FAILED) {
        return NULL;
    }

    /* Protect bottom guard page */
    if(mprotect(region, guard_size, PROT_NONE) == -1) {
        munmap(region, total_size);
        return NULL;
    }

    /* Protect top guard page */
    if(mprotect((char *)region + guard_size + aligned_stack_size, guard_size, PROT_NONE) == -1) {
        munmap(region, total_size);
        return NULL;
    }

    stack_base = (char *)region + guard_size;
#endif

    /* Allocate metadata separately (not on stack, so it can't be corrupted) */
    fcontext_stack_t *meta = (fcontext_stack_t *)malloc(sizeof(fcontext_stack_t));
    if(!meta) {
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, total_size);
#endif
        return NULL;
    }

    /* Stack grows downward, so stack_top is at the end of the stack region */
    void *stack_top = (char *)stack_base + aligned_stack_size;

    /* Align stack_top to FCONTEXT_STACK_ALIGNMENT boundary (round down) */
    stack_top = fcontext_align_stack_pointer(stack_top);

    /* Initialize metadata */
    meta->magic = FCONTEXT_STACK_MAGIC;
    meta->type = FCONTEXT_ALLOC_VMEM;
    meta->total_size = total_size;
    meta->stack_size = aligned_stack_size;
    meta->stack_top = stack_top;
    meta->watermark_enabled = false;
    meta->region = region;  /* Store mmap'd/VirtualAlloc'd region for cleanup */

    return meta;
}

/* ========================================================================
 * Stack Destruction
 * ======================================================================== */

/**
 * Destroy a stack and free all associated memory.
 */
void fcontext_stack_destroy(fcontext_stack_t *stack) {
    if(!stack) { return; }

    /* Validate magic number */
    if(stack->magic != FCONTEXT_STACK_MAGIC) {
        fprintf(stderr, "fcontext_stack_destroy: Invalid stack metadata (bad magic)\n");
        return;
    }

    if(stack->type == FCONTEXT_ALLOC_MALLOC) {
        /* For malloc stacks, the metadata is at the start of the allocation.
         * The region field points back to the metadata, so we just free that. */
        free(stack->region);
    } else if(stack->type == FCONTEXT_ALLOC_VMEM) {
        /* For VMEM stacks, unmap/free the mmap'd region and free metadata */
#ifdef _WIN32
        VirtualFree(stack->region, 0, MEM_RELEASE);
#else
        munmap(stack->region, stack->total_size);
#endif
        free(stack);
    }
}

/* ========================================================================
 * Watermark Support
 * ======================================================================== */

/*
 * Fill the stack with watermark pattern for high water mark detection.
 * Returns true on success, false on error.
 */
bool fcontext_stack_fill_watermark(fcontext_stack_t *stack) {
    if(!stack) { return false; }

    if(stack->magic != FCONTEXT_STACK_MAGIC) {
        return false;
    }

    if(stack->watermark_enabled) {
        /* Already filled */
        return false;
    }

    /* Fill from stack_base to stack_top with watermark pattern */
    /* Stack grows downward, so stack_top is higher than stack_base */
    char *stack_base = (char *)stack->stack_top - stack->stack_size;
    memset(stack_base, FCONTEXT_STACK_WATERMARK, stack->stack_size);

    stack->watermark_enabled = true;
    return true;
}

/**
 * Get maximum stack usage (high water mark).
 *
 * Scans from bottom (lowest address) upward looking for the watermark pattern.
 * Returns the number of bytes that were overwritten (i.e., bytes used).
 */
size_t fcontext_max_stack_use(fcontext_stack_t *stack) {
    if(!stack || stack->magic != FCONTEXT_STACK_MAGIC) {
        return SIZE_MAX;
    }

    if(!stack->watermark_enabled) {
        return SIZE_MAX;  /* Watermark not enabled */
    }

    /* Stack grows downward. Stack base is lowest address, stack_top is highest.
     * The pattern fills from base to top. As the stack is used (grows down),
     * the pattern gets overwritten from the top downward.
     * We want to find how far down the pattern extends, i.e., how much was used.
     */
    unsigned char *stack_base = (unsigned char *)stack->stack_top - stack->stack_size;
    size_t unused = 0;

    /* Count bytes from the bottom that still have the watermark pattern */
    while(unused < stack->stack_size && stack_base[unused] == FCONTEXT_STACK_WATERMARK) {
        unused++;
    }

    return stack->stack_size - unused;
}

/**
 * Check if stack was underflowed (wrote below the stack area).
 * Only applicable to malloc stacks with guard zones.
 */
bool fcontext_stack_underflow(fcontext_stack_t *stack) {
    if(!stack || stack->magic != FCONTEXT_STACK_MAGIC) {
        return false;
    }

    /* For VMEM stacks, guard pages cause SIGSEGV, not a detected condition */
    if(stack->type == FCONTEXT_ALLOC_VMEM) {
        return false;
    }

    /* For MALLOC stacks, we could check guard zones if we stored them.
     * For now, just return false. This would require extra metadata.
     */
    return false;
}

/**
 * Check if stack was overflowed (wrote above the stack area).
 * Only applicable to malloc stacks with guard zones.
 */
bool fcontext_stack_overflow(fcontext_stack_t *stack) {
    if(!stack || stack->magic != FCONTEXT_STACK_MAGIC) {
        return false;
    }

    /* For VMEM stacks, guard pages cause SIGSEGV, not a detected condition */
    if(stack->type == FCONTEXT_ALLOC_VMEM) {
        return false;
    }

    /* For MALLOC stacks, we could check guard zones if we stored them.
     * For now, just return false. This would require extra metadata.
     */
    return false;
}
