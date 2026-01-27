/*
 * fcontext.h - Safe Fiber API
 *
 * High-level fiber/coroutine support built on portable assembly primitives.
 * The low-level context switching API is internal and not exposed.
 *
 * Thread Safety: Fibers are bound to their creating thread.
 * See fcontext_fiber_transfer_thread() for cross-thread migration.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Configuration
 * ======================================================================== */

#ifndef FCONTEXT_DEFAULT_STACK_SIZE
    #define FCONTEXT_DEFAULT_STACK_SIZE (24 * 1024)
#endif

/* ========================================================================
 * Types
 * ======================================================================== */

/* Opaque fiber handle */
typedef struct fcontext_fiber fcontext_fiber_t;

/* Fiber execution state */
typedef enum {
    FCONTEXT_FIBER_CREATED,    /* Initialized, never started */
    FCONTEXT_FIBER_RUNNING,    /* Currently executing */
    FCONTEXT_FIBER_SUSPENDED,  /* Yielded, can be resumed */
    FCONTEXT_FIBER_FINISHED    /* Entry returned, cannot resume */
} fcontext_fiber_state_t;

/* Fiber entry function - receives initial data, returns final result */
typedef void *(*fcontext_fiber_fn_t)(void *initial_data);

/* ========================================================================
 * Fiber Creation
 * ======================================================================== */

/*
 * Create a fiber with virtual memory stack.
 * Includes guard pages for overflow/underflow detection (causes SIGSEGV/AV).
 *
 * Parameters:
 *   stack_size - Requested stack size (rounded up to page boundary)
 *   entry      - Function called when fiber first runs
 *
 * Returns:
 *   New fiber in CREATED state, or NULL on allocation failure
 */
extern fcontext_fiber_t *fcontext_fiber_create_vmem(
    size_t stack_size,
    fcontext_fiber_fn_t entry
);

/*
 * Create a fiber with malloc-allocated stack.
 * No guard pages. Use fcontext_fiber_stack_overflow() to check bounds.
 *
 * Parameters:
 *   stack_size - Requested stack size (rounded to alignment)
 *   entry      - Function called when fiber first runs
 *
 * Returns:
 *   New fiber in CREATED state, or NULL on allocation failure
 */
extern fcontext_fiber_t *fcontext_fiber_create_malloc(
    size_t stack_size,
    fcontext_fiber_fn_t entry
);

/* ========================================================================
 * Thread-to-Fiber Conversion
 * ======================================================================== */

/*
 * Convert the current thread into a fiber.
 *
 * Called implicitly by fcontext_fiber_switch() when needed.
 * Call explicitly for control over lifecycle or to get the handle.
 *
 * Returns:
 *   Fiber representing current thread, or NULL on error.
 *   Returns existing handle if already converted (idempotent).
 */
extern fcontext_fiber_t *fcontext_fiber_convert_thread(void);

/*
 * Check if current thread has been converted to a fiber.
 */
extern bool fcontext_fiber_is_thread_converted(void);

/*
 * Destroy the current thread's fiber context.
 *
 * Call when done using fibers from this thread.
 * Must not be called while fibers may yield back to this thread.
 */
extern void fcontext_fiber_destroy_thread_fiber(void);

/* ========================================================================
 * Fiber Switching
 * ======================================================================== */

/*
 * Switch to a fiber.
 *
 * If called from a non-fiber context, implicitly converts thread to fiber.
 * The target fiber receives the data via its entry function (first switch)
 * or as the return value from yield (subsequent switches).
 *
 * Safe to call repeatedly with the same fiber pointer.
 *
 * Parameters:
 *   fiber - Fiber to switch to (must be CREATED or SUSPENDED, same thread)
 *   data  - User data passed to target fiber
 *
 * Returns:
 *   Data passed back when target yields or finishes (entry function return value).
 *   Returns NULL if fiber is NULL, wrong thread, or invalid state.
 *   For finished fibers, returns cached return value (no-op).
 *
 * Note: NULL is a valid data value. Check fiber state to distinguish
 *       from errors if NULL data is expected.
 */
extern void *fcontext_fiber_switch(fcontext_fiber_t *fiber, void *data);

/*
 * Yield from current fiber back to caller.
 *
 * Returns to whoever most recently switched to this fiber.
 * Must be called from within a fiber, not from an unconverted thread.
 *
 * Parameters:
 *   data - User data passed back to caller
 *
 * Returns:
 *   Data passed on next switch to this fiber.
 *   Returns NULL if called from non-fiber context (programming error).
 */
extern void *fcontext_fiber_yield(void *data);

/* ========================================================================
 * Fiber State and Ownership
 * ======================================================================== */

/* Get fiber's current state */
extern fcontext_fiber_state_t fcontext_fiber_get_state(fcontext_fiber_t *fiber);

/* Get currently executing fiber (NULL if not in a fiber) */
extern fcontext_fiber_t *fcontext_fiber_current(void);

/*
 * Get the fiber that most recently switched to the current fiber.
 *
 * Useful when a fiber needs to know its caller for routing decisions.
 * Returns NULL if called from non-fiber or if current fiber is the thread fiber.
 */
extern fcontext_fiber_t *fcontext_fiber_get_caller(void);

/* Get thread ID that owns this fiber */
extern uint64_t fcontext_fiber_get_owner_thread(fcontext_fiber_t *fiber);

/*
 * Transfer fiber ownership to current thread.
 *
 * Fiber must be CREATED or SUSPENDED (not running or finished).
 * Clears the fiber's caller - yield will return NULL until next switch.
 *
 * Returns:
 *   true on success, false if invalid state or NULL
 */
extern bool fcontext_fiber_transfer_thread(fcontext_fiber_t *fiber);

/* ========================================================================
 * Stack Debugging (Watermark)
 * ======================================================================== */

/*
 * Fill stack with watermark pattern for usage tracking.
 *
 * Call after creation, before first switch.
 * For vmem fibers, causes physical memory allocation.
 *
 * Returns:
 *   true on success, false if NULL, wrong state, or thread fiber
 */
extern bool fcontext_fiber_fill_watermark(fcontext_fiber_t *fiber);

/*
 * Get maximum stack bytes used (high water mark).
 *
 * Requires prior fcontext_fiber_fill_watermark() call.
 * Fiber must be SUSPENDED or FINISHED.
 *
 * Returns:
 *   Bytes used, or SIZE_MAX on error
 */
extern size_t fcontext_fiber_get_stack_usage(fcontext_fiber_t *fiber);

/* Get total usable stack size (0 for thread fibers) */
extern size_t fcontext_fiber_get_stack_size(fcontext_fiber_t *fiber);

/* Check for stack overflow (malloc stacks only; vmem will SIGSEGV) */
extern bool fcontext_fiber_stack_overflow(fcontext_fiber_t *fiber);

/* Check for stack underflow (malloc stacks only) */
extern bool fcontext_fiber_stack_underflow(fcontext_fiber_t *fiber);

/* ========================================================================
 * Cleanup
 * ======================================================================== */

/*
 * Destroy a fiber and free its resources.
 *
 * Do not call on thread fibers - use fcontext_fiber_destroy_thread_fiber().
 * Do not call on a RUNNING fiber.
 * Safe to call on NULL.
 *
 * Returns:
 *   Cached result from fiber (or NULL if never ran)
 */
extern void *fcontext_fiber_destroy(fcontext_fiber_t *fiber);

/* ========================================================================
 * Utilities
 * ======================================================================== */

/* Get system page size in bytes */
extern size_t fcontext_get_page_size(void);

#ifdef __cplusplus
}
#endif
