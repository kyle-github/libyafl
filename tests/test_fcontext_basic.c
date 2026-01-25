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

static int test_phase = 0;

/**
 * Fiber function - entry point for new context
 */
fcontext_transfer_t fiber_func(fcontext_transfer_t t) {
    printf("  [fiber] entered context\n");
    fflush(stdout);
    assert(test_phase == 0);
    test_phase = 1;

    printf("  [fiber] yielding back to main\n");
    fflush(stdout);
    t = jump_fcontext(t.prev_context, (void *)0x1234);

    printf("  [fiber] resumed from main\n");
    fflush(stdout);
    assert(test_phase == 2);
    test_phase = 3;

    printf("  [fiber] finishing\n");
    fflush(stdout);

    /* Return to trampoline to switch back */
    return t;
}

int main(void) {
    printf("=== fcontext Basic Test ===\n");
    printf("[main] page size: %zu bytes\n", fcontext_get_page_size());

    /* Create context with guarded stack using mmap */
    fcontext_stack_t *state = fcontext_create(24 * 1024, fiber_func);
    if(state == NULL) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }
    printf("[main] created fiber context with guard pages\n");

    /* First switch: Enter fiber for first time */
    printf("[main] switching to fiber...\n");
    fcontext_transfer_t t = jump_fcontext(state->context, NULL);
    printf("[main] fiber returned to us\n");

    /* Verify fiber executed first phase */
    assert(test_phase == 1);
    assert(t.data == (void *)0x1234);
    printf("[main] fiber reached phase 1 correctly\n");

    /* Resume fiber */
    printf("[main] resuming fiber...\n");
    test_phase = 2;
    t = jump_fcontext(t.prev_context, NULL);
    printf("[main] fiber returned again\n");

    /* Verify fiber completed */
    assert(test_phase == 3);
    printf("[main] fiber reached phase 3 correctly\n");

    fcontext_destroy(state);
    printf("[main] destroyed context and freed guarded stack\n");
    printf("\n✓ PASS: Basic context creation and switching works\n");
    fflush(stdout);
    return 0;
}
