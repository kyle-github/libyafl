/*
 * yafl.c - Safe Fiber API Implementation
 *
 * Implements high-level fiber operations using low-level context switching.
 * Stack allocation with optional guard pages and watermark support.
 *
 * Derived from Boost.Context (https://github.com/boostorg/context)
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

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

#include "yafl.h"

/* ========================================================================
 * Low-Level API (Internal Only)
 * ======================================================================== */

/* Raw context handle - opaque pointer to saved machine state */
typedef struct yafl_opaque_t *yafl_t;

/* Raw transfer between contexts */
typedef struct {
    yafl_t prev_context;
    void *data;
} yafl_transfer_t;

/* Raw entry function type for low-level API */
typedef void (*yafl_entry_t)(yafl_transfer_t);

/* Low-level assembly-implemented functions */
extern yafl_t make_fcontext(void *sp, size_t size, yafl_entry_t fn);
extern yafl_transfer_t jump_fcontext(yafl_t const to, void *vp);

/* ========================================================================
 * Constants and Types
 * ======================================================================== */

#define FCONTEXT_FIBER_MAGIC 0xF1BE7001
#define FCONTEXT_STACK_WATERMARK 0xA5
#define FCONTEXT_STACK_ALIGNMENT 16

typedef enum {
    FCONTEXT_ALLOC_MALLOC,
    FCONTEXT_ALLOC_VMEM
} yafl_alloc_type_t;

/* Internal fiber structure */
struct yafl_fiber {
    uint32_t magic;
    yafl_alloc_type_t alloc_type;
    yafl_fiber_status_t status;

    /* Stack management */
    void *stack_region;
    size_t stack_total_size;
    void *stack_top;
    size_t stack_size;
    bool watermark_filled;

    /* Context tracking */
    yafl_t context;            /* Fiber's saved context */
    yafl_t resumer_context;    /* Context to return to */

    /* User entry and result */
    yafl_fiber_fn user_entry;
    void *cached_result;
    void *pending_arg;         /* Argument for next resume */
};

/* Typedef for internal use */
typedef struct yafl_fiber yafl_fiber_t;

/* Thread-local storage */
static _Thread_local yafl_fiber_t *tls_current_fiber = NULL;

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

static size_t get_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 0) {
        return 4096;
    }
    return (size_t)page_size;
#endif
}

static void *align_stack_pointer(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void *)(addr & ~(FCONTEXT_STACK_ALIGNMENT - 1));
}

/* ========================================================================
 * Trampoline: Adapts Low-Level API to High-Level Fiber API
 * ======================================================================== */

/*
 * This is called as the entry function by make_fcontext().
 * It wraps the user's entry function, manages state, and handles the result.
 */
static void fiber_entry_trampoline(yafl_transfer_t t) {
    yafl_fiber_t *fiber = (yafl_fiber_t *)t.data;

    /* Save resumer's context (who called resume on us) */
    fiber->resumer_context = t.prev_context;

    /* Update status and TLS */
    fiber->status = YAFL_FIBER_STATUS_RUNNING;
    tls_current_fiber = fiber;

    /* Call user entry with argument from first resume */
    void *result = fiber->user_entry(fiber->pending_arg);

    /* Mark complete and cache result */
    fiber->status = YAFL_FIBER_STATUS_COMPLETE;
    fiber->cached_result = result;
    tls_current_fiber = NULL;

    /* Return to resumer with final result */
    jump_fcontext(fiber->resumer_context, result);

    /* Should never reach here */
}

/* ========================================================================
 * Fiber Creation
 * ======================================================================== */

extern yafl_fiber_t *yafl_fiber_create(yafl_fiber_fn fiber_fn, size_t stack_size,
                                       yafl_stack_flags_t flags) {
    /* Validate fiber function is not NULL */
    if (fiber_fn == NULL) {
        return NULL;
    }

    /* Validate exactly one allocation type is set */
    int alloc_flags = flags & (YAFL_STACK_FLAGS_MALLOC | YAFL_STACK_FLAGS_VMEM);
    if (alloc_flags != YAFL_STACK_FLAGS_MALLOC && alloc_flags != YAFL_STACK_FLAGS_VMEM) {
        return NULL;
    }

    bool use_vmem = (flags & YAFL_STACK_FLAGS_VMEM) != 0;
    bool use_watermark = (flags & YAFL_STACK_FLAGS_WATERMARK) != 0;

    /* Allocate fiber structure */
    yafl_fiber_t *fiber = malloc(sizeof(yafl_fiber_t));
    if (fiber == NULL) {
        return NULL;
    }

    /* Initialize fiber structure */
    fiber->magic = FCONTEXT_FIBER_MAGIC;
    fiber->alloc_type = use_vmem ? FCONTEXT_ALLOC_VMEM : FCONTEXT_ALLOC_MALLOC;
    fiber->status = YAFL_FIBER_STATUS_SUSPENDED;
    fiber->user_entry = fiber_fn;
    fiber->cached_result = NULL;
    fiber->pending_arg = NULL;
    fiber->watermark_filled = use_watermark;
    fiber->context = NULL;
    fiber->resumer_context = NULL;

    /* Use default stack size if not specified */
    if (stack_size == 0) {
        stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
    }

    /* Allocate stack based on allocation type */
    if (use_vmem) {
        size_t page_size = get_page_size();
        size_t stack_with_overhead = stack_size + 256;
        size_t aligned_stack_size = ((stack_with_overhead + page_size - 1) / page_size) * page_size;
        size_t guard_size = page_size;
        size_t total_size = guard_size + aligned_stack_size + guard_size;

        void *region = NULL;
#ifdef _WIN32
        region = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
        if (region == NULL) {
            free(fiber);
            return NULL;
        }
        void *stack_base = (char *)region + guard_size;
        if (!VirtualAlloc(stack_base, aligned_stack_size, MEM_COMMIT, PAGE_READWRITE)) {
            VirtualFree(region, 0, MEM_RELEASE);
            free(fiber);
            return NULL;
        }
#else
        region = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (region == MAP_FAILED) {
            free(fiber);
            return NULL;
        }
        if (mprotect(region, guard_size, PROT_NONE) == -1) {
            munmap(region, total_size);
            free(fiber);
            return NULL;
        }
        if (mprotect((char *)region + guard_size + aligned_stack_size, guard_size, PROT_NONE) == -1) {
            munmap(region, total_size);
            free(fiber);
            return NULL;
        }
        void *stack_base = (char *)region + guard_size;
#endif

        void *stack_region_end = (char *)stack_base + aligned_stack_size;
        void *stack_top = (char *)stack_region_end - 256;
        stack_top = align_stack_pointer(stack_top);
        size_t actual_stack_size = (char *)stack_top - (char *)stack_base;

        fiber->stack_region = region;
        fiber->stack_total_size = total_size;
        fiber->stack_top = stack_top;
        fiber->stack_size = actual_stack_size;
    } else {
        /* malloc allocation */
        size_t allocated_size = stack_size + 256;
        void *block = malloc(allocated_size);
        if (block == NULL) {
            free(fiber);
            return NULL;
        }

        void *block_end = (char *)block + allocated_size;
        void *stack_top = (char *)block_end - 256;
        stack_top = align_stack_pointer(stack_top);

        size_t actual_stack_size = (char *)stack_top - (char *)block;

        fiber->stack_region = block;
        fiber->stack_total_size = allocated_size;
        fiber->stack_top = stack_top;
        fiber->stack_size = actual_stack_size;
    }

    /* Initialize the low-level context with trampoline as entry */
    fiber->context = make_fcontext(fiber->stack_top, fiber->stack_size, fiber_entry_trampoline);
    if (fiber->context == NULL) {
        if (use_vmem) {
#ifdef _WIN32
            VirtualFree(fiber->stack_region, 0, MEM_RELEASE);
#else
            munmap(fiber->stack_region, fiber->stack_total_size);
#endif
        } else {
            free(fiber->stack_region);
        }
        free(fiber);
        return NULL;
    }

    /* Apply watermark if requested */
    if (use_watermark) {
        memset((char *)fiber->stack_top - fiber->stack_size, FCONTEXT_STACK_WATERMARK, fiber->stack_size);
        /* Reinitialize context after watermark (overwrites filled area) */
        fiber->context = make_fcontext(fiber->stack_top, fiber->stack_size, fiber_entry_trampoline);
        if (fiber->context == NULL) {
            if (use_vmem) {
#ifdef _WIN32
                VirtualFree(fiber->stack_region, 0, MEM_RELEASE);
#else
                munmap(fiber->stack_region, fiber->stack_total_size);
#endif
            } else {
                free(fiber->stack_region);
            }
            free(fiber);
            return NULL;
        }
    }

    return fiber;
}

/* ========================================================================
 * Fiber Control Flow
 * ======================================================================== */

extern void *yafl_fiber_resume(yafl_fiber_t *fiber, void *arg) {
    /* Validation */
    if (fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return NULL;
    }

    /* If complete, return cached result (idempotent) */
    if (fiber->status == YAFL_FIBER_STATUS_COMPLETE) {
        return fiber->cached_result;
    }

    /* Cannot resume running fiber */
    if (fiber->status == YAFL_FIBER_STATUS_RUNNING) {
        return NULL;
    }

    /* Store argument for delivery */
    fiber->pending_arg = arg;

    /* Update status and TLS */
    fiber->status = YAFL_FIBER_STATUS_RUNNING;
    tls_current_fiber = fiber;

    /* Pass fiber pointer to trampoline on first resume */
    void *transfer_data = (void *)fiber;

    /* Perform context switch */
    yafl_transfer_t t = jump_fcontext(fiber->context, transfer_data);

    /* Back in resumer - update fiber's context for next resume */
    fiber->context = t.prev_context;
    tls_current_fiber = NULL;

    return t.data;
}

extern void *yafl_fiber_suspend(void *result) {
    yafl_fiber_t *current = tls_current_fiber;

    /* Must be in a fiber */
    if (current == NULL) {
        return NULL;
    }

    /* Update status */
    current->status = YAFL_FIBER_STATUS_SUSPENDED;

    /* Switch back to resumer */
    yafl_transfer_t t = jump_fcontext(current->resumer_context, result);

    /* When resumed - restore state */
    current->status = YAFL_FIBER_STATUS_RUNNING;
    current->resumer_context = t.prev_context;
    tls_current_fiber = current;

    /* Return argument passed to resume */
    return current->pending_arg;
}

/* ========================================================================
 * Fiber Status and Monitoring
 * ======================================================================== */

extern yafl_fiber_status_t yafl_fiber_status(yafl_fiber_t *fiber) {
    if (fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return YAFL_FIBER_STATUS_ERR;
    }
    return fiber->status;
}

extern size_t yafl_fiber_stack_high_watermark(yafl_fiber_t *fiber) {
    if (fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC || !fiber->watermark_filled) {
        return 0;
    }

    /* Scan from stack base for watermark bytes */
    unsigned char *stack_base = (unsigned char *)fiber->stack_top - fiber->stack_size;
    size_t unused = 0;

    while (unused < fiber->stack_size && stack_base[unused] == FCONTEXT_STACK_WATERMARK) {
        unused++;
    }

    return fiber->stack_size - unused;
}

/* ========================================================================
 * Cleanup
 * ======================================================================== */

extern void yafl_fiber_destroy(yafl_fiber_t *fiber) {
    if (fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return;
    }

    /* Cannot destroy running fiber */
    if (fiber->status == YAFL_FIBER_STATUS_RUNNING) {
        return;
    }

    /* Free stack based on allocation type */
    if (fiber->alloc_type == FCONTEXT_ALLOC_MALLOC) {
        free(fiber->stack_region);
    } else if (fiber->alloc_type == FCONTEXT_ALLOC_VMEM) {
#ifdef _WIN32
        VirtualFree(fiber->stack_region, 0, MEM_RELEASE);
#else
        munmap(fiber->stack_region, fiber->stack_total_size);
#endif
    }

    /* Invalidate and free */
    fiber->magic = 0;
    free(fiber);
}

/* ========================================================================
 * Utilities
 * ======================================================================== */

extern size_t yafl_get_page_size(void) {
    return get_page_size();
}
