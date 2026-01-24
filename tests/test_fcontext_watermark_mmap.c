/**
 * test_fcontext_watermark_mmap.c
 * Tests stack watermark checking with mmap-allocated stacks.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fcontext.h"

/* Test data to pass to coroutine */
typedef struct {
    int recursive_depth;
} test_data_t;

/* Recursive function to consume stack space */
static void consume_stack(int depth) {
    volatile char buffer[1024];
    memset((void*)buffer, depth & 0xFF, sizeof(buffer));
    if (depth > 0) {
        consume_stack(depth - 1);
    }
    (void)buffer[512]; /* Prevent tail-call optimization */
}

/* Coroutine entry point */
static void test_watermark_fiber(fcontext_transfer_t t) {
    test_data_t *data = (test_data_t *)t.data;
    printf("  [fiber] Consuming stack at depth %d\n", data->recursive_depth);
    consume_stack(data->recursive_depth);
    printf("  [fiber] Finished consuming stack\n");
    fcontext_swap(t.prev_context, NULL);
}

int main(void) {
    printf("=== mmap Stack Watermark Test ===\n\n");

    /* Test 1: Small stack usage */
    printf("Test 1: Small stack usage (depth 5)\n");
    test_data_t test_small = { .recursive_depth = 5 };
    /* Use default mmap allocation */
    fcontext_stack_t *ctx_small = fcontext_create(16 * 1024, test_watermark_fiber);
    if (!ctx_small) {
        fprintf(stderr, "Failed to create small context\n");
        return 1;
    }

    fcontext_swap(ctx_small->context, &test_small);

    size_t used_small = fcontext_get_stack_usage(ctx_small);
    printf("  Stack usage: %zu bytes\n", used_small);
    fcontext_destroy(ctx_small);
    printf("\n");

    /* Test 2: Large stack usage */
    printf("Test 2: Large stack usage (depth 15)\n");
    test_data_t test_large = { .recursive_depth = 15 };
    /* Use explicit mmap allocation */
    fcontext_stack_t *ctx_large = fcontext_create_mmap(24 * 1024, 4096, test_watermark_fiber);
    if (!ctx_large) {
        fprintf(stderr, "Failed to create large context\n");
        return 1;
    }

    fcontext_swap(ctx_large->context, &test_large);

    size_t used_large = fcontext_get_stack_usage(ctx_large);
    printf("  Stack usage: %zu bytes\n", used_large);
    printf("  Difference from small test: %zu bytes\n", used_large - used_small);
    fcontext_destroy(ctx_large);
    printf("\n");

    printf("✓ PASS: mmap stack watermark tests completed.\n");

    return 0;
}