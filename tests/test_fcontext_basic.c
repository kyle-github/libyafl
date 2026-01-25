/**
 * test_fcontext_basic.c
 * Basic low-level fcontext API test
 *
 * Tests the fundamental context creation and switching operations.
 * Demonstrates asymmetric coroutine pattern.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "fcontext.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int32_t test_phase = 0;

/**
 * Fiber function - entry point for new context
 */
void fiber_func(fcontext_transfer_t t) {
    printf("  [fiber] entered context\n");
    fflush(stdout);
    assert(test_phase == 0);
    test_phase = 1;

    printf("  [fiber] yielding back to main\n");
    fflush(stdout);
    t = fcontext_switch(t.prev_context, (void *)0x1234);

    printf("  [fiber] resumed from main\n");
    fflush(stdout);
    assert(test_phase == 2);
    test_phase = 3;

    printf("  [fiber] finishing\n");
    fflush(stdout);
}

int main(void) {
    printf("=== fcontext Basic Test ===\n");
    printf("[main] page size: %zu bytes\n", fcontext_get_page_size());

    /* Allocate stack with guard pages using vmem */
    fcontext_stack_t *state = fcontext_vmem_stack(24 * 1024);
    if(state == NULL) {
        fprintf(stderr, "Failed to allocate stack\n");
        return 1;
    }
    printf("[main] allocated fiber stack with guard pages\n");

    /* Initialize context */
    state->context = fcontext_init(state->stack_top, state->stack_size, fiber_func);
    printf("[main] initialized fiber context\n");

    /* First switch: Enter fiber for first time */
    printf("[main] switching to fiber...\n");
    fcontext_transfer_t t = fcontext_switch(state->context, NULL);
    printf("[main] fiber returned to us\n");

    /* Verify fiber executed first phase */
    assert(test_phase == 1);
    assert(t.data == (void *)0x1234);
    printf("[main] fiber reached phase 1 correctly\n");

    /* Resume fiber */
    printf("[main] resuming fiber...\n");
    test_phase = 2;
    t = fcontext_switch(t.prev_context, NULL);
    printf("[main] fiber returned again\n");

    /* Verify fiber completed */
    assert(test_phase == 3);
    printf("[main] fiber reached phase 3 correctly\n");

    fcontext_stack_destroy(state);
    printf("[main] destroyed context and freed guarded stack\n");
    printf("\n✓ PASS: Basic context creation and switching works\n");
    fflush(stdout);
    return 0;
}
