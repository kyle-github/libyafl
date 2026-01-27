/**
 * test_fcontext_low_level.c
 * Low-level fiber test
 *
 * Tests basic fiber operations.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void *low_level_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[fiber] low level test\n");
    fflush(stderr);
    return (void *)0xEE;
}

int main(void) {
    fprintf(stderr, "=== fcontext Low-Level Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_malloc(24 * 1024, low_level_fiber);
    assert(fiber != NULL);

    void *result = fcontext_fiber_switch(fiber, NULL);
    assert(result == (void *)0xEE);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "✓ PASS: Low-level test passed\n");
    fflush(stderr);
    return 0;
}
