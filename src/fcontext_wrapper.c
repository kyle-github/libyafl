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

    /* Allocate requested size plus 256 bytes for metadata and alignment overhead */
    size_t allocated_size = stack_size + 256;
    void *block = malloc(allocated_size);
    if(!block) {
        return NULL;
    }

    /* Calculate preliminary stack_top by leaving space for metadata at the end */
    void *block_end = (char *)block + allocated_size;
    void *stack_top = (char *)block_end - sizeof(fcontext_stack_t);

    /* Align stack_top down to 16-byte boundary */
    stack_top = fcontext_align_stack_pointer(stack_top);

    /* The metadata structure is stored at stack_top (in the reserved area above usable stack) */
    fcontext_stack_t *meta = (fcontext_stack_t *)stack_top;

    /* Actual usable stack size is from block start to stack_top */
    size_t actual_stack_size = (char *)stack_top - (char *)block;

    /* Initialize metadata */
    meta->magic = FCONTEXT_STACK_MAGIC;
    meta->type = FCONTEXT_ALLOC_MALLOC;
    meta->total_size = allocated_size;
    meta->stack_size = actual_stack_size;
    meta->stack_top = stack_top;
    meta->watermark_enabled = false;
    meta->region = block;  /* Store pointer to original allocation for cleanup */

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

    /* Add 256 bytes for metadata and alignment overhead, then round up to page boundary */
    size_t stack_with_overhead = stack_size + 256;
    size_t aligned_stack_size = ((stack_with_overhead + page_size - 1) / page_size) * page_size;

    /* One guard page on each side */
    size_t guard_size = page_size;
    size_t total_size = guard_size + aligned_stack_size + guard_size;

    void *region = NULL;
    void *stack_base = NULL;

#ifdef _WIN32
    /* Windows: Reserve entire region as inaccessible, then commit stack as readable/writable */
    /* Reserve address space for the entire region (guard + stack + guard) as PAGE_NOACCESS */
    region = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
    if(!region) {
        return NULL;
    }

    /* Commit and set readable/writable only the stack portion (between the guard pages) */
    void *stack_commit_start = (char *)region + guard_size;
    if(!VirtualAlloc(stack_commit_start, aligned_stack_size, MEM_COMMIT, PAGE_READWRITE)) {
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

    /* Calculate preliminary stack_top: end of stack region minus metadata space */
    void *stack_region_end = (char *)stack_base + aligned_stack_size;
    void *stack_top = (char *)stack_region_end - sizeof(fcontext_stack_t);

    /* Align stack_top down to 16-byte boundary */
    stack_top = fcontext_align_stack_pointer(stack_top);

    /* Actual usable stack size is from stack_base to aligned stack_top */
    size_t actual_stack_size = (char *)stack_top - (char *)stack_base;

    /* The metadata is stored at stack_top, within the allocated region */
    fcontext_stack_t *meta = (fcontext_stack_t *)stack_top;

    /* Initialize metadata */
    meta->magic = FCONTEXT_STACK_MAGIC;
    meta->type = FCONTEXT_ALLOC_VMEM;
    meta->total_size = total_size;
    meta->stack_size = actual_stack_size;
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
        /* For malloc stacks, the metadata is within the allocation.
         * The region field points to the original malloc'd block. */
        free(stack->region);
    } else if(stack->type == FCONTEXT_ALLOC_VMEM) {
        /* For VMEM stacks, the metadata is stored within the mmap'd/VirtualAlloc'd region.
         * Only unmap/free the region itself - don't separately free the metadata. */
#ifdef _WIN32
        VirtualFree(stack->region, 0, MEM_RELEASE);
#else
        munmap(stack->region, stack->total_size);
#endif
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
