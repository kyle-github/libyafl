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
static fcontext_transfer_t test_watermark_fiber(fcontext_transfer_t t) {
    test_data_t *data = (test_data_t *)t.data;
    printf("  [fiber] Consuming stack at depth %d\n", data->recursive_depth);
    consume_stack(data->recursive_depth);
    printf("  [fiber] Finished consuming stack\n");
    return t;
}

/* Helper to check software guard zones */
static int check_canaries(const fcontext_stack_t *ctx) {
    if(!ctx || ctx->alloc_type != FCONTEXT_ALLOC_MALLOC || ctx->guard_size == 0) { return 1; /* No guards to check */ }
    unsigned char *guard_bottom = (unsigned char *)ctx->stack_base - ctx->guard_size;
    unsigned char *guard_top = (unsigned char *)ctx->stack_base + ctx->stack_size;

    for(size_t i = 0; i < ctx->guard_size; i++) {
        if(guard_bottom[i] != 0xCD) { return 0; /* Bottom guard corrupted */ }
        if(guard_top[i] != 0xCD) { return 0; /* Top guard corrupted */ }
    }
    return 1;
}

int main(void) {
    printf("=== malloc Stack Watermark and Canary Test ===\n\n");

    test_data_t test_data = {.recursive_depth = 10};
    fcontext_stack_t *ctx = fcontext_create_malloc(16 * 1024, 1024, test_watermark_fiber);
    if(!ctx) {
        fprintf(stderr, "Failed to create malloc context\n");
        return 1;
    }

    fcontext_swap(ctx->context, &test_data);

    if(!check_canaries(ctx)) {
        printf("  ✗ FAIL: Guard zones (canaries) were corrupted!\n");
        fcontext_destroy(ctx);
        return 1;
    }
    printf("  ✓ Guard zones intact\n");

    size_t used = fcontext_get_stack_usage(ctx);
    printf("  Stack usage: %zu / %zu bytes\n", used, ctx->stack_size);
    fcontext_destroy(ctx);

    printf("\n✓ PASS: malloc stack watermark and canary tests completed.\n");
    return 0;
}