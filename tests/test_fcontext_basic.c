/**
 * test_fcontext_basic.c
 * Basic fiber API test
 *
 * Tests fundamental fiber creation and switching operations.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int32_t test_phase = 0;

void *fiber_func(void *initial_data) {
    (void)initial_data;
    fprintf(stderr, "[fiber] entered\n");
    fflush(stderr);
    assert(test_phase == 0);
    test_phase = 1;

    fprintf(stderr, "[fiber] yielding back to main\n");
    fflush(stderr);
    void *data = fcontext_fiber_yield((void *)0x1234);

    fprintf(stderr, "[fiber] resumed from main with data: %p\n", data);
    fflush(stderr);
    assert(test_phase == 2);
    test_phase = 3;

    fprintf(stderr, "[fiber] finishing\n");
    fflush(stderr);
    return (void *)0x5678;
}

int main(void) {
    fprintf(stderr, "=== fcontext Basic Fiber Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, fiber_func);
    assert(fiber != NULL);
    fprintf(stderr, "[main] created fiber\n");
    fflush(stderr);

    fprintf(stderr, "[main] switching to fiber...\n");
    fflush(stderr);
    void *result = fcontext_fiber_switch(fiber, NULL);
    fprintf(stderr, "[main] fiber yielded: %p\n", result);
    fflush(stderr);

    assert(test_phase == 1);
    assert(result == (void *)0x1234);
    fprintf(stderr, "[main] fiber reached phase 1 correctly\n");
    fflush(stderr);

    fprintf(stderr, "[main] resuming fiber...\n");
    fflush(stderr);
    test_phase = 2;
    result = fcontext_fiber_switch(fiber, NULL);
    fprintf(stderr, "[main] fiber finished: %p\n", result);
    fflush(stderr);

    assert(test_phase == 3);
    assert(result == (void *)0x5678);
    fprintf(stderr, "[main] fiber reached phase 3 correctly\n");
    fflush(stderr);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();
    fprintf(stderr, "[main] destroyed fiber\n");
    fflush(stderr);

    fprintf(stderr, "\n✓ PASS: Basic fiber creation and switching works\n");
    fflush(stderr);
    return 0;
}
