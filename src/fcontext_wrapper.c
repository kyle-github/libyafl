/**
 * fcontext_wrapper.c
 * Safe fiber API implementation
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
    #include <pthread.h>
#endif

/* ========================================================================
 * Low-Level API (Internal Only)
 * ======================================================================== */

/* Raw context handle - opaque pointer to saved machine state */
typedef struct fcontext_opaque_t *fcontext_t;

/* Raw transfer between contexts */
typedef struct {
    fcontext_t prev_context;
    void *data;
} fcontext_transfer_t;

/* Raw entry function type for low-level API */
typedef void (*fcontext_entry_t)(fcontext_transfer_t);

/* Low-level assembly-implemented functions */
extern fcontext_t fcontext_init(void *sp, size_t size, fcontext_entry_t fn);
extern fcontext_transfer_t fcontext_switch(fcontext_t const to, void *vp);

/* ========================================================================
 * Constants and Types
 * ======================================================================== */

#define FCONTEXT_FIBER_MAGIC 0xF1BE7001
#define FCONTEXT_STACK_MAGIC 0x5A5A5A5A
#define FCONTEXT_STACK_WATERMARK 0xA5
#define FCONTEXT_STACK_ALIGNMENT 16

typedef enum {
    FCONTEXT_ALLOC_MALLOC,
    FCONTEXT_ALLOC_VMEM,
    FCONTEXT_ALLOC_THREAD    /* Thread fiber, no stack to free */
} fcontext_alloc_type_t;

/* Internal fiber structure */
struct fcontext_fiber {
    uint32_t magic;
    fcontext_alloc_type_t alloc_type;
    fcontext_fiber_state_t state;
    uint64_t owner_thread;

    /* Stack management */
    void *stack_region;
    size_t stack_total_size;
    void *stack_top;
    size_t stack_size;
    bool watermark_filled;

    /* Context tracking */
    fcontext_t context;              /* Current context handle (my paused state) */
    struct fcontext_fiber *caller;   /* Caller fiber */

    /* User entry and result */
    fcontext_fiber_fn_t user_entry;
    void *cached_result;
    void *initial_data;  /* Data from first switch */
    bool first_run;      /* Track if this is the first run */
};

/* Typedef for internal use */
typedef struct fcontext_fiber fcontext_fiber_t;

/* Thread-local storage */
static _Thread_local fcontext_fiber_t *tls_current_fiber = NULL;
static _Thread_local fcontext_fiber_t *tls_thread_fiber = NULL;

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
    if(page_size <= 0) { return 4096; }
    return (size_t)page_size;
#endif
}

static void *align_stack_pointer(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void *)(addr & ~(FCONTEXT_STACK_ALIGNMENT - 1));
}

static uint64_t get_current_thread_id(void) {
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

/* ========================================================================
 * Trampoline: Adapts Low-Level API to High-Level Fiber API
 * ======================================================================== */

/*
 * This is called as the entry function by fcontext_init().
 * It wraps the user's entry function, manages state, and handles the result.
 */
static void fiber_entry_trampoline(fcontext_transfer_t t) {
    fcontext_fiber_t *fiber = (fcontext_fiber_t *)t.data;

    fprintf(stderr, "[fiber] entry trampoline: fiber=%p, initial_data=%p, prev_context=%p\n", (void *)fiber, fiber->initial_data, (void *)t.prev_context);
    fflush(stderr);

    /* Initialize caller's context so yields can use it */
    fiber->caller->context = t.prev_context;
    fprintf(stderr, "[fiber] initialized caller->context: %p\n", (void *)fiber->caller->context);
    fflush(stderr);

    /* Set up fiber state */
    fiber->state = FCONTEXT_FIBER_RUNNING;
    fiber->first_run = false;
    tls_current_fiber = fiber;

    fprintf(stderr, "[fiber] calling user entry function with data: %p\n", fiber->initial_data);
    fflush(stderr);

    /* Call user's entry function - it receives the actual initial data, not the fiber pointer */
    void *result = fiber->user_entry(fiber->initial_data);

    fprintf(stderr, "[fiber] user entry returned: %p\n", result);
    fflush(stderr);

    /* Cache the result and mark as finished */
    fiber->cached_result = result;
    fiber->state = FCONTEXT_FIBER_FINISHED;
    tls_current_fiber = NULL;

    fprintf(stderr, "[fiber] switching back to caller with result: %p\n", result);
    fflush(stderr);

    /* Switch back to caller using caller's context */
    fcontext_switch(fiber->caller->context, result);

    fprintf(stderr, "[fiber] ERROR: should not return from final switch\n");
    fflush(stderr);
}

/* ========================================================================
 * Fiber Creation (Internal Helper)
 * ======================================================================== */

static fcontext_fiber_t *fiber_alloc(
    fcontext_alloc_type_t alloc_type,
    size_t stack_size,
    fcontext_fiber_fn_t entry
) {
    if(entry == NULL) {
        return NULL;
    }

    fcontext_fiber_t *fiber = malloc(sizeof(fcontext_fiber_t));
    if(!fiber) {
        return NULL;
    }

    fiber->magic = FCONTEXT_FIBER_MAGIC;
    fiber->alloc_type = alloc_type;
    fiber->state = FCONTEXT_FIBER_CREATED;
    fiber->owner_thread = get_current_thread_id();
    fiber->context = NULL;
    fiber->caller = NULL;
    fiber->user_entry = entry;
    fiber->cached_result = NULL;
    fiber->watermark_filled = false;
    fiber->initial_data = NULL;
    fiber->first_run = true;

    fprintf(stderr, "[fiber_alloc] created fiber: %p, type=%d\n", (void *)fiber, alloc_type);
    fflush(stderr);

    if(alloc_type == FCONTEXT_ALLOC_THREAD) {
        /* Thread fibers have no stack */
        fiber->stack_region = NULL;
        fiber->stack_total_size = 0;
        fiber->stack_top = NULL;
        fiber->stack_size = 0;
        return fiber;
    }

    /* Allocate stack */
    if(alloc_type == FCONTEXT_ALLOC_MALLOC) {
        if(stack_size == 0) {
            stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
        }

        size_t allocated_size = stack_size + 256;
        void *block = malloc(allocated_size);
        if(!block) {
            free(fiber);
            return NULL;
        }

        void *block_end = (char *)block + allocated_size;
        void *stack_top = (char *)block_end - 256;  /* Reserve space for metadata */
        stack_top = align_stack_pointer(stack_top);

        size_t actual_stack_size = (char *)stack_top - (char *)block;

        fiber->stack_region = block;
        fiber->stack_total_size = allocated_size;
        fiber->stack_top = stack_top;
        fiber->stack_size = actual_stack_size;

    } else if(alloc_type == FCONTEXT_ALLOC_VMEM) {
        size_t page_size = get_page_size();
        if(stack_size == 0) {
            stack_size = FCONTEXT_DEFAULT_STACK_SIZE;
        }

        size_t stack_with_overhead = stack_size + 256;
        size_t aligned_stack_size = ((stack_with_overhead + page_size - 1) / page_size) * page_size;
        size_t guard_size = page_size;
        size_t total_size = guard_size + aligned_stack_size + guard_size;

        void *region = NULL;
#ifdef _WIN32
        region = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
        if(!region) {
            free(fiber);
            return NULL;
        }
        void *stack_base = (char *)region + guard_size;
        if(!VirtualAlloc(stack_base, aligned_stack_size, MEM_COMMIT, PAGE_READWRITE)) {
            VirtualFree(region, 0, MEM_RELEASE);
            free(fiber);
            return NULL;
        }
#else
        region = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(region == MAP_FAILED) {
            free(fiber);
            return NULL;
        }
        if(mprotect(region, guard_size, PROT_NONE) == -1) {
            munmap(region, total_size);
            free(fiber);
            return NULL;
        }
        if(mprotect((char *)region + guard_size + aligned_stack_size, guard_size, PROT_NONE) == -1) {
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
    }

    fprintf(stderr, "[fiber_alloc] allocated stack: top=%p, size=%zu\n", fiber->stack_top, fiber->stack_size);
    fflush(stderr);

    /* Initialize the low-level context with trampoline as entry */
    fiber->context = fcontext_init(fiber->stack_top, fiber->stack_size, fiber_entry_trampoline);
    if(!fiber->context) {
        free(fiber->stack_region);
        free(fiber);
        return NULL;
    }

    fprintf(stderr, "[fiber_alloc] initialized context: %p\n", (void *)fiber->context);
    fflush(stderr);

    return fiber;
}

/* ========================================================================
 * Public Fiber API
 * ======================================================================== */

extern fcontext_fiber_t *fcontext_fiber_create_vmem(
    size_t stack_size,
    fcontext_fiber_fn_t entry
) {
    fprintf(stderr, "[create_vmem] creating fiber with vmem stack\n");
    fflush(stderr);
    return fiber_alloc(FCONTEXT_ALLOC_VMEM, stack_size, entry);
}

extern fcontext_fiber_t *fcontext_fiber_create_malloc(
    size_t stack_size,
    fcontext_fiber_fn_t entry
) {
    fprintf(stderr, "[create_malloc] creating fiber with malloc stack\n");
    fflush(stderr);
    return fiber_alloc(FCONTEXT_ALLOC_MALLOC, stack_size, entry);
}

extern fcontext_fiber_t *fcontext_fiber_convert_thread(void) {
    fprintf(stderr, "[convert_thread] converting current thread\n");
    fflush(stderr);

    if(tls_thread_fiber != NULL) {
        fprintf(stderr, "[convert_thread] already converted: %p\n", (void *)tls_thread_fiber);
        fflush(stderr);
        return tls_thread_fiber;
    }

    fcontext_fiber_t *fiber = malloc(sizeof(fcontext_fiber_t));
    if(!fiber) {
        return NULL;
    }

    fiber->magic = FCONTEXT_FIBER_MAGIC;
    fiber->alloc_type = FCONTEXT_ALLOC_THREAD;
    fiber->state = FCONTEXT_FIBER_RUNNING;
    fiber->owner_thread = get_current_thread_id();
    fiber->context = NULL;
    fiber->caller = NULL;
    fiber->user_entry = NULL;
    fiber->cached_result = NULL;
    fiber->watermark_filled = false;
    fiber->stack_region = NULL;
    fiber->stack_total_size = 0;
    fiber->stack_top = NULL;
    fiber->stack_size = 0;
    fiber->initial_data = NULL;
    fiber->first_run = false;

    tls_thread_fiber = fiber;
    tls_current_fiber = fiber;

    fprintf(stderr, "[convert_thread] created thread fiber: %p\n", (void *)fiber);
    fflush(stderr);

    return fiber;
}

extern bool fcontext_fiber_is_thread_converted(void) {
    return tls_thread_fiber != NULL;
}

extern void fcontext_fiber_destroy_thread_fiber(void) {
    fprintf(stderr, "[destroy_thread_fiber] destroying thread fiber\n");
    fflush(stderr);

    if(tls_thread_fiber) {
        free(tls_thread_fiber);
        tls_thread_fiber = NULL;
        tls_current_fiber = NULL;
    }
}

extern void *fcontext_fiber_switch(fcontext_fiber_t *fiber, void *data) {
    fprintf(stderr, "[switch] switching to fiber: %p, data=%p\n", (void *)fiber, data);
    fflush(stderr);

    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        fprintf(stderr, "[switch] error: fiber is NULL or invalid\n");
        fflush(stderr);
        return NULL;
    }

    if(fiber->owner_thread != get_current_thread_id()) {
        fprintf(stderr, "[switch] error: fiber owned by different thread\n");
        fflush(stderr);
        return NULL;
    }

    /* If fiber is finished, just return cached result (no-op) */
    if(fiber->state == FCONTEXT_FIBER_FINISHED) {
        fprintf(stderr, "[switch] fiber already finished, returning cached result: %p\n", fiber->cached_result);
        fflush(stderr);
        return fiber->cached_result;
    }

    /* If fiber is running, error */
    if(fiber->state == FCONTEXT_FIBER_RUNNING) {
        fprintf(stderr, "[switch] error: fiber already running\n");
        fflush(stderr);
        return NULL;
    }

    /* Convert current thread to fiber if needed */
    fcontext_fiber_t *caller = tls_current_fiber;
    if(caller == NULL) {
        fprintf(stderr, "[switch] auto-converting thread to fiber\n");
        fflush(stderr);
        caller = fcontext_fiber_convert_thread();
        if(caller == NULL) {
            fprintf(stderr, "[switch] error: could not convert thread\n");
            fflush(stderr);
            return NULL;
        }
    }

    /* Set up caller/callee relationship */
    fiber->caller = caller;

    fprintf(stderr, "[switch] switching: caller=%p, target=%p\n", (void *)caller, (void *)fiber);
    fflush(stderr);

    /* Do the low-level switch */
    /* For first switch (CREATED), store the actual data in fiber and pass fiber pointer for trampoline */
    /* For subsequent switches (SUSPENDED), pass the actual data */
    if(fiber->state == FCONTEXT_FIBER_CREATED) {
        fprintf(stderr, "[switch] storing initial data: %p\n", data);
        fflush(stderr);
        fiber->initial_data = data;
    }
    void *switch_data = (fiber->state == FCONTEXT_FIBER_CREATED) ? (void *)fiber : data;

    fcontext_transfer_t t = fcontext_switch(fiber->context, switch_data);

    fprintf(stderr, "[switch] returned to caller: prev_context=%p, data=%p\n", (void *)t.prev_context, t.data);
    fflush(stderr);

    /* Update fiber's context for next switch */
    fiber->context = t.prev_context;

    /* Update caller's context so fiber can yield back to us */
    caller->context = t.prev_context;
    fprintf(stderr, "[switch] updated caller->context to: %p\n", (void *)caller->context);
    fflush(stderr);

    fprintf(stderr, "[switch] fiber state after return: %d\n", fiber->state);
    fflush(stderr);

    return t.data;
}

extern void *fcontext_fiber_yield(void *data) {
    fprintf(stderr, "[yield] yielding with data=%p\n", data);
    fflush(stderr);

    fcontext_fiber_t *current = tls_current_fiber;
    if(current == NULL) {
        fprintf(stderr, "[yield] error: not in a fiber context\n");
        fflush(stderr);
        return NULL;
    }

    /* Change state to SUSPENDED before yielding */
    current->state = FCONTEXT_FIBER_SUSPENDED;

    fprintf(stderr, "[yield] switching to caller: %p, caller->context=%p\n", (void *)current->caller, (void *)current->caller->context);
    fflush(stderr);

    fcontext_transfer_t t = fcontext_switch(current->caller->context, data);

    fprintf(stderr, "[yield] resumed with data=%p, prev_context=%p\n", t.data, (void *)t.prev_context);
    fflush(stderr);

    /* Back to RUNNING after resume */
    current->state = FCONTEXT_FIBER_RUNNING;

    /* Update caller's context for next yield */
    current->caller->context = t.prev_context;
    fprintf(stderr, "[yield] updated caller->context to: %p\n", (void *)current->caller->context);
    fflush(stderr);

    return t.data;
}

extern fcontext_fiber_state_t fcontext_fiber_get_state(fcontext_fiber_t *fiber) {
    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return FCONTEXT_FIBER_CREATED;
    }
    return fiber->state;
}

extern fcontext_fiber_t *fcontext_fiber_current(void) {
    return tls_current_fiber;
}

extern fcontext_fiber_t *fcontext_fiber_get_caller(void) {
    fcontext_fiber_t *current = tls_current_fiber;
    if(current == NULL || current->alloc_type == FCONTEXT_ALLOC_THREAD) {
        return NULL;
    }
    return current->caller;
}

extern uint64_t fcontext_fiber_get_owner_thread(fcontext_fiber_t *fiber) {
    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return 0;
    }
    return fiber->owner_thread;
}

extern bool fcontext_fiber_transfer_thread(fcontext_fiber_t *fiber) {
    fprintf(stderr, "[transfer_thread] transferring fiber to current thread\n");
    fflush(stderr);

    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return false;
    }

    if(fiber->state == FCONTEXT_FIBER_RUNNING || fiber->state == FCONTEXT_FIBER_FINISHED) {
        fprintf(stderr, "[transfer_thread] error: invalid fiber state\n");
        fflush(stderr);
        return false;
    }

    fiber->owner_thread = get_current_thread_id();
    fiber->caller = NULL;

    fprintf(stderr, "[transfer_thread] fiber transferred\n");
    fflush(stderr);

    return true;
}

extern bool fcontext_fiber_fill_watermark(fcontext_fiber_t *fiber) {
    fprintf(stderr, "[fill_watermark] filling watermark for fiber: %p\n", (void *)fiber);
    fflush(stderr);

    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return false;
    }

    if(fiber->state != FCONTEXT_FIBER_CREATED) {
        fprintf(stderr, "[fill_watermark] error: fiber not in CREATED state\n");
        fflush(stderr);
        return false;
    }

    if(fiber->alloc_type == FCONTEXT_ALLOC_THREAD) {
        return false;
    }

    if(fiber->watermark_filled) {
        return false;
    }

    /* Fill the watermark */
    memset((char *)fiber->stack_top - fiber->stack_size, FCONTEXT_STACK_WATERMARK, fiber->stack_size);
    fiber->watermark_filled = true;

    fprintf(stderr, "[fill_watermark] watermark filled, reinitializing context\n");
    fflush(stderr);

    /* Reinitialize the context since we overwrote it with the watermark */
    fiber->context = fcontext_init(fiber->stack_top, fiber->stack_size, fiber_entry_trampoline);
    if(!fiber->context) {
        fprintf(stderr, "[fill_watermark] error: failed to reinitialize context\n");
        fflush(stderr);
        return false;
    }

    fprintf(stderr, "[fill_watermark] context reinitialized\n");
    fflush(stderr);

    return true;
}

extern size_t fcontext_fiber_get_stack_usage(fcontext_fiber_t *fiber) {
    fprintf(stderr, "[get_stack_usage] getting stack usage for fiber: %p\n", (void *)fiber);
    fflush(stderr);

    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return SIZE_MAX;
    }

    if(!fiber->watermark_filled) {
        return SIZE_MAX;
    }

    unsigned char *stack_base = (unsigned char *)fiber->stack_top - fiber->stack_size;
    size_t unused = 0;

    while(unused < fiber->stack_size && stack_base[unused] == FCONTEXT_STACK_WATERMARK) {
        unused++;
    }

    return fiber->stack_size - unused;
}

extern size_t fcontext_fiber_get_stack_size(fcontext_fiber_t *fiber) {
    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return 0;
    }
    return fiber->stack_size;
}

extern bool fcontext_fiber_stack_overflow(fcontext_fiber_t *fiber) {
    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return false;
    }

    if(fiber->alloc_type != FCONTEXT_ALLOC_MALLOC) {
        return false;
    }

    return false;
}

extern bool fcontext_fiber_stack_underflow(fcontext_fiber_t *fiber) {
    if(fiber == NULL || fiber->magic != FCONTEXT_FIBER_MAGIC) {
        return false;
    }

    if(fiber->alloc_type != FCONTEXT_ALLOC_MALLOC) {
        return false;
    }

    return false;
}

extern void *fcontext_fiber_destroy(fcontext_fiber_t *fiber) {
    fprintf(stderr, "[destroy] destroying fiber: %p\n", (void *)fiber);
    fflush(stderr);

    if(fiber == NULL) {
        return NULL;
    }

    if(fiber->magic != FCONTEXT_FIBER_MAGIC) {
        fprintf(stderr, "[destroy] error: invalid fiber magic\n");
        fflush(stderr);
        return NULL;
    }

    if(fiber->alloc_type == FCONTEXT_ALLOC_THREAD) {
        fprintf(stderr, "[destroy] error: cannot destroy thread fiber\n");
        fflush(stderr);
        return NULL;
    }

    void *result = fiber->cached_result;

    if(fiber->alloc_type == FCONTEXT_ALLOC_MALLOC) {
        free(fiber->stack_region);
    } else if(fiber->alloc_type == FCONTEXT_ALLOC_VMEM) {
#ifdef _WIN32
        VirtualFree(fiber->stack_region, 0, MEM_RELEASE);
#else
        munmap(fiber->stack_region, fiber->stack_total_size);
#endif
    }

    fiber->magic = 0;  /* Invalidate */
    free(fiber);

    fprintf(stderr, "[destroy] fiber destroyed, result=%p\n", result);
    fflush(stderr);

    return result;
}

extern size_t fcontext_get_page_size(void) {
    return get_page_size();
}
