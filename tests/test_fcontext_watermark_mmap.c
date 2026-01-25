/**
 * test_fcontext_watermark_mmap.c
 * Tests stack watermark checking with mmap-allocated stacks.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test data to pass to coroutine */
typedef struct {
    int recursive_depth;
} test_data_t;

/* Recursive function to consume stack space */
static void consume_stack(int depth) {
    volatile char buffer[1024];
    memset((void *)buffer, depth & 0xFF, sizeof(buffer));
    if(depth > 0) { consume_stack(depth - 1); }
    (void)buffer[512]; /* Prevent tail-call optimization */
}

/* Coroutine entry point */
static void test_watermark_fiber(fcontext_transfer_t t) {
    test_data_t *data = (test_data_t *)t.data;
    printf("  [fiber] Consuming stack at depth %d\n", data->recursive_depth);
    consume_stack(data->recursive_depth);
    printf("  [fiber] Finished consuming stack\n");
}

int main(void) {
    printf("=== mmap Stack Watermark Test ===\n\n");

    /* Test 1: Small stack usage */
    printf("Test 1: Small stack usage (depth 5)\n");
    test_data_t test_small = {.recursive_depth = 5};

    fcontext_stack_t *ctx_small = fcontext_vmem_stack(16 * 1024);
    if(!ctx_small) {
        fprintf(stderr, "Failed to allocate small stack\n");
        return 1;
    }

    /* Fill watermark to enable high water mark detection */
    if(!fcontext_stack_fill_watermark(ctx_small)) {
        fprintf(stderr, "Failed to fill watermark for small stack\n");
        fcontext_stack_destroy(ctx_small);
        return 1;
    }

    /* Initialize context */
    ctx_small->context = fcontext_init(ctx_small->stack_top, ctx_small->stack_size, test_watermark_fiber);
    printf("  [main] initialized small context\n");

    fcontext_swap(ctx_small->context, &test_small);

    size_t used_small = fcontext_max_stack_use(ctx_small);
    if(used_small != SIZE_MAX) {
        printf("  Stack usage: %zu bytes\n", used_small);
    } else {
        printf("  Stack usage: watermark not available\n");
    }
    fcontext_stack_destroy(ctx_small);
    printf("\n");

    /* Test 2: Large stack usage */
    printf("Test 2: Large stack usage (depth 15)\n");
    test_data_t test_large = {.recursive_depth = 15};

    fcontext_stack_t *ctx_large = fcontext_vmem_stack(24 * 1024);
    if(!ctx_large) {
        fprintf(stderr, "Failed to allocate large stack\n");
        return 1;
    }

    /* Fill watermark to enable high water mark detection */
    if(!fcontext_stack_fill_watermark(ctx_large)) {
        fprintf(stderr, "Failed to fill watermark for large stack\n");
        fcontext_stack_destroy(ctx_large);
        return 1;
    }

    /* Initialize context */
    ctx_large->context = fcontext_init(ctx_large->stack_top, ctx_large->stack_size, test_watermark_fiber);
    printf("  [main] initialized large context\n");

    fcontext_swap(ctx_large->context, &test_large);

    size_t used_large = fcontext_max_stack_use(ctx_large);
    if(used_large != SIZE_MAX) {
        printf("  Stack usage: %zu bytes\n", used_large);
        if(used_small != SIZE_MAX) {
            printf("  Difference from small test: %zu bytes\n", used_large - used_small);
        }
    } else {
        printf("  Stack usage: watermark not available\n");
    }
    fcontext_stack_destroy(ctx_large);
    printf("\n");

    printf("✓ PASS: mmap stack watermark tests completed.\n");

    return 0;
}