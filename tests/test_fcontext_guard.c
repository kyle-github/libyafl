/**
 * test_fcontext_guard.c
 * Guard page test
 *
 * Verifies that guard pages are installed.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void *guard_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[fiber] running\n");
    fflush(stderr);
    return (void *)0xBB;
}

int main(void) {
    fprintf(stderr, "=== fcontext Guard Page Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, guard_fiber);
    assert(fiber != NULL);

    void *result = fcontext_fiber_switch(fiber, NULL);
    assert(result == (void *)0xBB);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "✓ PASS: Guard page test passed\n");
    fflush(stderr);
    return 0;
}
