/**
 * test_yafl_watermark.c
 * Watermark test with both vmem and malloc stacks
 *
 * Verifies watermark functionality for stack usage tracking.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "yafl.h"

void *watermark_fiber(void *data) {
    (void)data;
    fprintf(stderr, "[fiber] running\n");
    fflush(stderr);

    /* Use some stack space */
    volatile char buf[1024];
    buf[0] = 1;
    buf[1023] = 2;

    return (void *)0xCC;
}

static void test_watermark(const char *name, yafl_fiber_t *fiber) {
    fprintf(stderr, "\n[test] %s watermark\n", name);
    fflush(stderr);

    /* Fill watermark before first run */
    bool filled = yafl_fiber_fill_watermark(fiber);
    assert(filled);
    fprintf(stderr, "[test] watermark filled\n");
    fflush(stderr);

    /* Run the fiber */
    void *result = yafl_fiber_switch(fiber, NULL);
    assert(result == (void *)0xCC);
    fprintf(stderr, "[test] fiber completed\n");
    fflush(stderr);

    /* Check stack usage */
    size_t used = yafl_fiber_get_stack_usage(fiber);
    size_t total = yafl_fiber_get_stack_size(fiber);

    fprintf(stderr, "[test] stack used: %zu / %zu bytes (%.1f%%)\n",
            used, total, (used * 100.0) / total);
    fflush(stderr);

    assert(used > 0);
    assert(used < total);
    assert(used >= 1024);  /* At least our buffer size */
}

int main(void) {
    fprintf(stderr, "=== yafl Watermark Test ===\n");
    fflush(stderr);

    /* Test with vmem allocation */
    yafl_fiber_t *fiber_vmem = yafl_fiber_create_vmem(24 * 1024, watermark_fiber);
    assert(fiber_vmem != NULL);
    test_watermark("vmem", fiber_vmem);
    yafl_fiber_destroy(fiber_vmem);

    /* Test with malloc allocation */
    yafl_fiber_t *fiber_malloc = yafl_fiber_create_malloc(24 * 1024, watermark_fiber);
    assert(fiber_malloc != NULL);
    test_watermark("malloc", fiber_malloc);
    yafl_fiber_destroy(fiber_malloc);

    yafl_fiber_destroy_thread_fiber();

    fprintf(stderr, "\nPASS: Watermark test passed for both allocation types\n");
    fflush(stderr);
    return 0;
}
