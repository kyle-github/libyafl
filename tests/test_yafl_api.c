/**
 * test_yafl_api.c
 * Comprehensive API test
 *
 * Tests all API functions, error conditions, and edge cases.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "yafl.h"

static int call_count = 0;

void *simple_fiber(void *data) {
    call_count++;
    fprintf(stderr, "[fiber] called with data: %p\n", data);
    fflush(stderr);
    return (void *)0xABCD;
}

void *null_data_fiber(void *data) {
    fprintf(stderr, "[fiber] received NULL data: %p\n", data);
    fflush(stderr);
    assert(data == NULL);
    return NULL;
}

void *yielding_fiber(void *data) {
    fprintf(stderr, "[fiber] first call, data: %p\n", data);
    fflush(stderr);
    assert(data == (void *)0x1);

    void *result = yafl_fiber_yield((void *)0x2);
    fprintf(stderr, "[fiber] resumed with: %p\n", result);
    fflush(stderr);
    assert(result == (void *)0x3);

    return (void *)0x4;
}

void *multi_yield_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[multi_yield] starting\n");
    fflush(stderr);

    /* Multiple consecutive yields */
    for(int i = 0; i < 5; i++) {
        fprintf(stderr, "[multi_yield] yield %d\n", i);
        fflush(stderr);
        yafl_fiber_yield((void *)(uintptr_t)(0x100 + i));
    }

    fprintf(stderr, "[multi_yield] finishing\n");
    fflush(stderr);
    return (void *)0x999;
}

int main(void) {
    fprintf(stderr, "=== yafl Comprehensive API Test ===\n");
    fflush(stderr);

    /* Test: yafl_get_page_size() */
    fprintf(stderr, "\n[test] yafl_get_page_size()\n");
    fflush(stderr);
    size_t page_size = yafl_get_page_size();
    fprintf(stderr, "[test] page size: %zu bytes\n", page_size);
    fflush(stderr);
    assert(page_size > 0);
    assert(page_size >= 4096);

    /* Test: Explicit thread conversion */
    fprintf(stderr, "\n[test] explicit thread conversion\n");
    fflush(stderr);
    assert(!yafl_fiber_is_thread_converted());
    yafl_fiber_t *thread_fiber = yafl_fiber_convert_thread();
    assert(thread_fiber != NULL);
    assert(yafl_fiber_is_thread_converted());
    assert(yafl_fiber_current() == thread_fiber);
    fprintf(stderr, "[test] thread converted: %p\n", (void *)thread_fiber);
    fflush(stderr);

    /* Test: Idempotent thread conversion */
    fprintf(stderr, "\n[test] idempotent thread conversion\n");
    fflush(stderr);
    yafl_fiber_t *thread_fiber2 = yafl_fiber_convert_thread();
    assert(thread_fiber2 == thread_fiber);
    fprintf(stderr, "[test] same fiber returned: %p\n", (void *)thread_fiber2);
    fflush(stderr);

    /* Test: Default stack size (0) */
    fprintf(stderr, "\n[test] default stack size\n");
    fflush(stderr);
    yafl_fiber_t *fiber_default = yafl_fiber_create_vmem(0, simple_fiber);
    assert(fiber_default != NULL);
    size_t default_size = yafl_fiber_get_stack_size(fiber_default);
    fprintf(stderr, "[test] default stack size: %zu bytes\n", default_size);
    fflush(stderr);
    assert(default_size > 0);

    /* Test: Fiber state transitions */
    fprintf(stderr, "\n[test] fiber state transitions\n");
    fflush(stderr);
    assert(yafl_fiber_get_state(fiber_default) == FCONTEXT_FIBER_CREATED);
    fprintf(stderr, "[test] state: CREATED\n");
    fflush(stderr);

    void *result = yafl_fiber_switch(fiber_default, (void *)0x99);
    assert(yafl_fiber_get_state(fiber_default) == FCONTEXT_FIBER_FINISHED);
    fprintf(stderr, "[test] state: FINISHED\n");
    fflush(stderr);
    assert(result == (void *)0xABCD);
    assert(call_count == 1);

    /* Test: Idempotent switch to finished fiber */
    fprintf(stderr, "\n[test] idempotent switch to finished fiber\n");
    fflush(stderr);
    result = yafl_fiber_switch(fiber_default, (void *)0x88);
    assert(result == (void *)0xABCD);  /* Cached result */
    assert(call_count == 1);  /* Not called again */
    fprintf(stderr, "[test] returned cached result, fiber not re-entered\n");
    fflush(stderr);

    /* Test: Destroy returns cached result */
    fprintf(stderr, "\n[test] destroy returns result\n");
    fflush(stderr);
    void *destroy_result = yafl_fiber_destroy(fiber_default);
    assert(destroy_result == (void *)0xABCD);
    fprintf(stderr, "[test] destroy returned: %p\n", destroy_result);
    fflush(stderr);

    /* Test: NULL data handling */
    fprintf(stderr, "\n[test] NULL as valid data\n");
    fflush(stderr);
    yafl_fiber_t *fiber_null = yafl_fiber_create_malloc(16 * 1024, null_data_fiber);
    assert(fiber_null != NULL);
    result = yafl_fiber_switch(fiber_null, NULL);
    assert(result == NULL);  /* NULL is valid return value */
    fprintf(stderr, "[test] NULL data passed and returned correctly\n");
    fflush(stderr);
    yafl_fiber_destroy(fiber_null);

    /* Test: yafl_fiber_get_caller() and yafl_fiber_current() */
    fprintf(stderr, "\n[test] get_caller() and current()\n");
    fflush(stderr);
    assert(yafl_fiber_current() == thread_fiber);
    assert(yafl_fiber_get_caller() == NULL);  /* Thread fiber has no caller */
    fprintf(stderr, "[test] current: %p, caller: NULL\n", (void *)yafl_fiber_current());
    fflush(stderr);

    /* Test: Yield and state changes */
    fprintf(stderr, "\n[test] yield and state changes\n");
    fflush(stderr);
    yafl_fiber_t *fiber_yield = yafl_fiber_create_vmem(16 * 1024, yielding_fiber);
    assert(fiber_yield != NULL);
    assert(yafl_fiber_get_state(fiber_yield) == FCONTEXT_FIBER_CREATED);

    result = yafl_fiber_switch(fiber_yield, (void *)0x1);
    assert(result == (void *)0x2);
    assert(yafl_fiber_get_state(fiber_yield) == FCONTEXT_FIBER_SUSPENDED);
    fprintf(stderr, "[test] fiber yielded, state: SUSPENDED\n");
    fflush(stderr);

    result = yafl_fiber_switch(fiber_yield, (void *)0x3);
    assert(result == (void *)0x4);
    assert(yafl_fiber_get_state(fiber_yield) == FCONTEXT_FIBER_FINISHED);
    fprintf(stderr, "[test] fiber finished, state: FINISHED\n");
    fflush(stderr);
    yafl_fiber_destroy(fiber_yield);

    /* Test: Error conditions - NULL fiber */
    fprintf(stderr, "\n[test] error: switch to NULL fiber\n");
    fflush(stderr);
    result = yafl_fiber_switch(NULL, NULL);
    assert(result == NULL);
    fprintf(stderr, "[test] NULL fiber handled correctly\n");
    fflush(stderr);

    /* Test: Error conditions - get_state on NULL */
    fprintf(stderr, "\n[test] error: get_state on NULL\n");
    fflush(stderr);
    yafl_fiber_state_t state = yafl_fiber_get_state(NULL);
    assert(state == FCONTEXT_FIBER_CREATED);  /* Safe default */
    fprintf(stderr, "[test] NULL fiber state handled correctly\n");
    fflush(stderr);

    /* Test: Error conditions - destroy NULL */
    fprintf(stderr, "\n[test] error: destroy NULL\n");
    fflush(stderr);
    destroy_result = yafl_fiber_destroy(NULL);
    assert(destroy_result == NULL);
    fprintf(stderr, "[test] destroy NULL handled correctly\n");
    fflush(stderr);

    /* Test: Watermark error conditions */
    fprintf(stderr, "\n[test] watermark error conditions\n");
    fflush(stderr);
    yafl_fiber_t *fiber_wm = yafl_fiber_create_vmem(16 * 1024, simple_fiber);
    assert(fiber_wm != NULL);

    /* Get usage without fill */
    size_t usage = yafl_fiber_get_stack_usage(fiber_wm);
    assert(usage == SIZE_MAX);  /* Error return */
    fprintf(stderr, "[test] usage without fill: SIZE_MAX\n");
    fflush(stderr);

    /* Fill watermark */
    bool filled = yafl_fiber_fill_watermark(fiber_wm);
    assert(filled);

    /* Try to fill again (should fail, already filled) */
    filled = yafl_fiber_fill_watermark(fiber_wm);
    assert(!filled);
    fprintf(stderr, "[test] watermark cannot be re-filled\n");
    fflush(stderr);

    yafl_fiber_switch(fiber_wm, NULL);
    usage = yafl_fiber_get_stack_usage(fiber_wm);
    assert(usage != SIZE_MAX && usage > 0);
    fprintf(stderr, "[test] usage after fill: %zu bytes\n", usage);
    fflush(stderr);
    yafl_fiber_destroy(fiber_wm);

    /* Test: Create fiber with NULL entry */
    fprintf(stderr, "\n[test] error: create fiber with NULL entry\n");
    fflush(stderr);
    yafl_fiber_t *fiber_null_entry = yafl_fiber_create_vmem(16 * 1024, NULL);
    assert(fiber_null_entry == NULL);
    fiber_null_entry = yafl_fiber_create_malloc(16 * 1024, NULL);
    assert(fiber_null_entry == NULL);
    fprintf(stderr, "[test] NULL entry rejected correctly\n");
    fflush(stderr);

    /* Test: Thread fiber operations */
    fprintf(stderr, "\n[test] thread fiber operations\n");
    fflush(stderr);

    /* Get stack size of thread fiber (should be 0) */
    size_t thread_stack_size = yafl_fiber_get_stack_size(thread_fiber);
    assert(thread_stack_size == 0);
    fprintf(stderr, "[test] thread fiber stack size: 0\n");
    fflush(stderr);

    /* Try to destroy thread fiber (should fail) */
    destroy_result = yafl_fiber_destroy(thread_fiber);
    assert(destroy_result == NULL);
    assert(yafl_fiber_is_thread_converted());  /* Still converted */
    fprintf(stderr, "[test] cannot destroy thread fiber via yafl_fiber_destroy()\n");
    fflush(stderr);

    /* Try to fill watermark on thread fiber (should fail) */
    filled = yafl_fiber_fill_watermark(thread_fiber);
    assert(!filled);
    fprintf(stderr, "[test] cannot fill watermark on thread fiber\n");
    fflush(stderr);

    /* Test: Default stack size for malloc */
    fprintf(stderr, "\n[test] malloc default stack size\n");
    fflush(stderr);
    yafl_fiber_t *fiber_malloc_default = yafl_fiber_create_malloc(0, simple_fiber);
    assert(fiber_malloc_default != NULL);
    size_t malloc_default_size = yafl_fiber_get_stack_size(fiber_malloc_default);
    fprintf(stderr, "[test] malloc default stack size: %zu bytes\n", malloc_default_size);
    fflush(stderr);
    assert(malloc_default_size > 0);
    yafl_fiber_switch(fiber_malloc_default, NULL);
    yafl_fiber_destroy(fiber_malloc_default);

    /* Test: Both allocation types */
    fprintf(stderr, "\n[test] both allocation types\n");
    fflush(stderr);
    yafl_fiber_t *fiber_vmem = yafl_fiber_create_vmem(16 * 1024, simple_fiber);
    yafl_fiber_t *fiber_malloc = yafl_fiber_create_malloc(16 * 1024, simple_fiber);
    assert(fiber_vmem != NULL);
    assert(fiber_malloc != NULL);
    yafl_fiber_switch(fiber_vmem, NULL);
    yafl_fiber_switch(fiber_malloc, NULL);
    yafl_fiber_destroy(fiber_vmem);
    yafl_fiber_destroy(fiber_malloc);
    fprintf(stderr, "[test] both vmem and malloc work\n");
    fflush(stderr);

    /* Test: Small stack size */
    fprintf(stderr, "\n[test] small stack size (2KB)\n");
    fflush(stderr);
    yafl_fiber_t *fiber_small = yafl_fiber_create_malloc(2 * 1024, simple_fiber);
    if(fiber_small != NULL) {
        yafl_fiber_switch(fiber_small, NULL);
        yafl_fiber_destroy(fiber_small);
        fprintf(stderr, "[test] small stack works\n");
        fflush(stderr);
    } else {
        fprintf(stderr, "[test] small stack allocation failed (acceptable)\n");
        fflush(stderr);
    }

    /* Test: Multiple consecutive yields */
    fprintf(stderr, "\n[test] multiple consecutive yields\n");
    fflush(stderr);
    yafl_fiber_t *fiber_multi = yafl_fiber_create_vmem(16 * 1024, multi_yield_fiber);
    assert(fiber_multi != NULL);

    for(int i = 0; i < 5; i++) {
        result = yafl_fiber_switch(fiber_multi, NULL);
        assert(result == (void *)(uintptr_t)(0x100 + i));
        fprintf(stderr, "[test] received yield %d: %p\n", i, result);
        fflush(stderr);
    }

    result = yafl_fiber_switch(fiber_multi, NULL);
    assert(result == (void *)0x999);
    fprintf(stderr, "[test] fiber finished after 5 yields\n");
    fflush(stderr);
    yafl_fiber_destroy(fiber_multi);

    /* Test: Watermark on SUSPENDED fiber */
    fprintf(stderr, "\n[test] watermark on suspended fiber\n");
    fflush(stderr);
    yafl_fiber_t *fiber_wm_susp = yafl_fiber_create_vmem(16 * 1024, multi_yield_fiber);
    assert(fiber_wm_susp != NULL);
    yafl_fiber_fill_watermark(fiber_wm_susp);

    /* Run once and suspend */
    yafl_fiber_switch(fiber_wm_susp, NULL);
    assert(yafl_fiber_get_state(fiber_wm_susp) == FCONTEXT_FIBER_SUSPENDED);

    /* Get stack usage while suspended */
    usage = yafl_fiber_get_stack_usage(fiber_wm_susp);
    assert(usage != SIZE_MAX && usage > 0);
    fprintf(stderr, "[test] stack usage while SUSPENDED: %zu bytes\n", usage);
    fflush(stderr);

    /* Finish the fiber */
    while(yafl_fiber_get_state(fiber_wm_susp) != FCONTEXT_FIBER_FINISHED) {
        yafl_fiber_switch(fiber_wm_susp, NULL);
    }
    yafl_fiber_destroy(fiber_wm_susp);

    /* Cleanup */
    yafl_fiber_destroy_thread_fiber();

    /* Test: Yield from non-fiber context (must be last, after thread fiber destroyed) */
    fprintf(stderr, "\n[test] error: yield from non-fiber context\n");
    fflush(stderr);
    result = yafl_fiber_yield((void *)0xBAD);
    assert(result == NULL);
    fprintf(stderr, "[test] yield from non-fiber context returned NULL\n");
    fflush(stderr);

    fprintf(stderr, "\nPASS: All API functions and error conditions tested\n");
    fflush(stderr);
    return 0;
}
