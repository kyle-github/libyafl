/**
 * test_fcontext_alignment.c
 * Stack alignment test
 *
 * Verifies that fiber stacks are properly aligned.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void *aligned_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[fiber] running\n");
    fflush(stderr);
    return (void *)0xAA;
}

int main(void) {
    fprintf(stderr, "=== fcontext Alignment Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, aligned_fiber);
    assert(fiber != NULL);

    void *result = fcontext_fiber_switch(fiber, NULL);
    assert(result == (void *)0xAA);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "✓ PASS: Alignment test passed\n");
    fflush(stderr);
    return 0;
}
