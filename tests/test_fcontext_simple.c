/**
 * test_fcontext_simple.c
 * Simple fiber API test
 *
 * Verifies basic fiber creation and entry.
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

static bool called = false;

void *simple_fiber(void *initial_data) {
    (void)initial_data;
    called = true;
    fprintf(stderr, "[fiber] entry function called\n");
    fflush(stderr);
    return (void *)0x42;
}

int main(void) {
    fprintf(stderr, "=== fcontext Simple Fiber Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, simple_fiber);
    assert(fiber != NULL);
    fprintf(stderr, "[main] created fiber\n");
    fflush(stderr);

    called = false;
    fprintf(stderr, "[main] entering fiber...\n");
    fflush(stderr);
    void *result = fcontext_fiber_switch(fiber, NULL);

    assert(called);
    assert(result == (void *)0x42);
    fprintf(stderr, "[main] fiber returned correctly\n");
    fflush(stderr);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "\n✓ PASS: Simple fiber entry works\n");
    fflush(stderr);
    return 0;
}
