/*
 * yafl.h - Safe Fiber API
 *
 * High-level fiber/coroutine support built on portable assembly primitives.
 * The low-level context switching API is internal and not exposed.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
typedef struct yafl_fiber yafl_fiber_t;

/* Fiber execution state */
typedef enum {
    FCONTEXT_FIBER_CREATED,   /* Initialized, never started */
    FCONTEXT_FIBER_RUNNING,   /* Currently executing */
    FCONTEXT_FIBER_SUSPENDED, /* Yielded, can be resumed */
    FCONTEXT_FIBER_FINISHED   /* Entry returned, cannot resume */
} yafl_fiber_state_t;

/* Fiber entry function - receives initial data, returns final result */
typedef void *(*yafl_fiber_fn_t)(void *initial_data);

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
extern yafl_fiber_t *yafl_fiber_create_vmem(size_t stack_size, yafl_fiber_fn_t entry);

/*
 * Create a fiber with malloc-allocated stack.
 * No guard pages (use vmem for overflow detection).
 *
 * Parameters:
 *   stack_size - Requested stack size (rounded to alignment)
 *   entry      - Function called when fiber first runs
 *
 * Returns:
 *   New fiber in CREATED state, or NULL on allocation failure
 */
extern yafl_fiber_t *yafl_fiber_create_malloc(size_t stack_size, yafl_fiber_fn_t entry);

/* ========================================================================
 * Thread-to-Fiber Conversion
 * ======================================================================== */

/*
 * Convert the current thread into a fiber.
 *
 * Called implicitly by yafl_fiber_switch() when needed.
 * Call explicitly for control over lifecycle or to get the handle.
 *
 * Returns:
 *   Fiber representing current thread, or NULL on error.
 *   Returns existing handle if already converted (idempotent).
 */
extern yafl_fiber_t *yafl_fiber_convert_thread(void);

/*
 * Check if current thread has been converted to a fiber.
 */
extern bool yafl_fiber_is_thread_converted(void);

/*
 * Destroy the current thread's fiber context.
 *
 * Call when done using fibers from this thread.
 * Must not be called while fibers may yield back to this thread.
 */
extern void yafl_fiber_destroy_thread_fiber(void);

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
 *   fiber - Fiber to switch to (must be CREATED or SUSPENDED)
 *   data  - User data passed to target fiber
 *
 * Returns:
 *   Data passed back when target yields or finishes (entry function return value).
 *   Returns NULL if fiber is NULL or invalid state.
 *   For finished fibers, returns cached return value (no-op).
 *
 * Note: NULL is a valid data value. Check fiber state to distinguish
 *       from errors if NULL data is expected.
 */
extern void *yafl_fiber_switch(yafl_fiber_t *fiber, void *data);

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
extern void *yafl_fiber_yield(void *data);

/* ========================================================================
 * Fiber State and Ownership
 * ======================================================================== */

/* Get fiber's current state */
extern yafl_fiber_state_t yafl_fiber_get_state(yafl_fiber_t *fiber);

/* Get currently executing fiber (NULL if not in a fiber) */
extern yafl_fiber_t *yafl_fiber_current(void);

/*
 * Get the fiber that most recently switched to the current fiber.
 *
 * Useful when a fiber needs to know its caller for routing decisions.
 * Returns NULL if called from non-fiber or if current fiber is the thread fiber.
 */
extern yafl_fiber_t *yafl_fiber_get_caller(void);

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
extern bool yafl_fiber_fill_watermark(yafl_fiber_t *fiber);

/*
 * Get maximum stack bytes used (high water mark).
 *
 * Requires prior yafl_fiber_fill_watermark() call.
 * Fiber must be SUSPENDED or FINISHED.
 *
 * Returns:
 *   Bytes used, or SIZE_MAX on error
 */
extern size_t yafl_fiber_get_stack_usage(yafl_fiber_t *fiber);

/* Get total usable stack size (0 for thread fibers) */
extern size_t yafl_fiber_get_stack_size(yafl_fiber_t *fiber);

/* ========================================================================
 * Cleanup
 * ======================================================================== */

/*
 * Destroy a fiber and free its resources.
 *
 * Do not call on thread fibers - use yafl_fiber_destroy_thread_fiber().
 * Do not call on a RUNNING fiber.
 * Safe to call on NULL.
 *
 * Returns:
 *   Cached result from fiber (or NULL if never ran)
 */
extern void *yafl_fiber_destroy(yafl_fiber_t *fiber);

/* ========================================================================
 * Utilities
 * ======================================================================== */

/* Get system page size in bytes */
extern size_t yafl_get_page_size(void);

#ifdef __cplusplus
}
#endif
