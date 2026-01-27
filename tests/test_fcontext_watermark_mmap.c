/**
 * test_fcontext_watermark_mmap.c
 * Watermark test with vmem stacks
 *
 * Verifies watermark functionality.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void *watermark_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[fiber] running\n");
    fflush(stderr);
    char buf[512];
    buf[0] = 1;
    return (void *)0xCC;
}

int main(void) {
    fprintf(stderr, "=== fcontext Watermark (mmap) Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, watermark_fiber);
    assert(fiber != NULL);

    fcontext_fiber_fill_watermark(fiber);

    void *result = fcontext_fiber_switch(fiber, NULL);
    assert(result == (void *)0xCC);

    size_t used = fcontext_fiber_get_stack_usage(fiber);
    fprintf(stderr, "[main] stack used: %zu bytes\n", used);
    fflush(stderr);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "✓ PASS: Watermark test passed\n");
    fflush(stderr);
    return 0;
}
