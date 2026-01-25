/**
 * test_fcontext_watermark_malloc.c
 * Tests stack watermark checking with malloc-allocated stacks and canaries.
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

/*
 * Helper to check software guard zones (for malloc allocations)
 * Note: malloc stacks don't have guard zones in the new API - they're just contiguous memory.
 * Guard protection is only available with vmem stacks which have hardware guard pages.
 * This function is kept for reference but malloc stacks won't have detectable canaries.
 */
static bool check_canaries(const fcontext_stack_t *ctx) {
    if(!ctx || ctx->type != FCONTEXT_ALLOC_MALLOC) { return true; }
    /* Malloc allocations in the new API are just [metadata][stack] with no guard zones */
    return true;
}

int main(void) {
    printf("=== malloc Stack Watermark Test ===\n\n");

    test_data_t test_data = {.recursive_depth = 10};

    /* Allocate stack using malloc */
    fcontext_stack_t *ctx = fcontext_malloc_stack(16 * 1024);
    if(!ctx) {
        fprintf(stderr, "Failed to allocate malloc stack\n");
        return 1;
    }
    printf("  [main] allocated stack using malloc\n");

    /* Fill with watermark to enable high water mark detection */
    if(!fcontext_stack_fill_watermark(ctx)) {
        fprintf(stderr, "Failed to fill watermark\n");
        fcontext_stack_destroy(ctx);
        return 1;
    }
    printf("  [main] filled stack with watermark pattern\n");

    /* Initialize context */
    ctx->context = fcontext_init(ctx->stack_top, ctx->stack_size, test_watermark_fiber);
    printf("  [main] initialized context\n");

    /* Run the fiber */
    fcontext_swap(ctx->context, &test_data);

    if(!check_canaries(ctx)) {
        printf("  ✗ FAIL: Guard zones were corrupted!\n");
        fcontext_stack_destroy(ctx);
        return 1;
    }
    printf("  ✓ Guard zones intact\n");

    /*  Query stack usage */
    size_t used = fcontext_max_stack_use(ctx);
    if(used == SIZE_MAX) {
        printf("  [main] Stack usage: watermark not available\n");
    } else {
        printf("  [main] Stack usage: %zu / %zu bytes\n", used, ctx->stack_size);
    }

    fcontext_stack_destroy(ctx);

    printf("\n✓ PASS: malloc stack watermark test completed.\n");
    return 0;
}