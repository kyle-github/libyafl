/**
 * test_fcontext_watermark.c
 * Tests stack watermark checking and alignment features
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fcontext.h"

/* Test data to pass to coroutine */
typedef struct {
    int recursive_depth;
    size_t expected_usage;
} test_data_t;

/* Recursive function to consume stack space */
static void consume_stack(int depth) {
    /* Allocate a chunk on the stack */
    volatile char buffer[1024];

    /* Touch the buffer to ensure it's allocated */
    memset((void*)buffer, depth & 0xFF, sizeof(buffer));

    if (depth > 0) {
        consume_stack(depth - 1);
    }

    /* Prevent tail-call optimization */
    (void)buffer[512];
}

/* Coroutine entry point */
static void test_watermark_fiber(fcontext_transfer_t t) {
    test_data_t *data = (test_data_t *)t.data;

    printf("Fiber: consuming stack at depth %d\n", data->recursive_depth);

    /* Consume stack space */
    consume_stack(data->recursive_depth);

    printf("Fiber: finished consuming stack\n");

    /* Jump back to caller */
    jump_fcontext(t.prev_context, NULL);
}

int main(void) {
    printf("=== Stack Watermark and Alignment Test ===\n\n");

    /* Test 1: Stack alignment helper */
    printf("Test 1: Stack alignment helper\n");
    void *unaligned = (void *)0x12345;
    void *aligned = fcontext_align_stack_pointer(unaligned);
    printf("  Unaligned: %p\n", unaligned);
    printf("  Aligned:   %p\n", aligned);
    printf("  Alignment: %s\n\n",
           ((uintptr_t)aligned % FCONTEXT_STACK_ALIGNMENT == 0) ? "PASS" : "FAIL");

    /* Test 2: Small stack usage */
    printf("Test 2: Small stack usage (depth 5)\n");
    test_data_t test_small = { .recursive_depth = 5 };
    fcontext_stack_t *ctx_small = fcontext_create(16 * 1024, test_watermark_fiber);
    if (!ctx_small) {
        fprintf(stderr, "Failed to create small context\n");
        return 1;
    }

    /* Run the fiber */
    jump_fcontext(ctx_small->context, &test_small);

    /* Check stack usage before destroying */
    size_t used_small = fcontext_get_stack_usage(ctx_small);
    printf("  Stack usage: %zu bytes\n", used_small);

    /* Destroy will also print usage */
    printf("  Destroying context:\n  ");
    fcontext_destroy(ctx_small);
    printf("\n");

    /* Test 3: Large stack usage */
    printf("Test 3: Large stack usage (depth 15)\n");
    test_data_t test_large = { .recursive_depth = 15 };
    fcontext_stack_t *ctx_large = fcontext_create(24 * 1024, test_watermark_fiber);
    if (!ctx_large) {
        fprintf(stderr, "Failed to create large context\n");
        return 1;
    }

    /* Run the fiber */
    jump_fcontext(ctx_large->context, &test_large);

    /* Check stack usage before destroying */
    size_t used_large = fcontext_get_stack_usage(ctx_large);
    printf("  Stack usage: %zu bytes\n", used_large);
    printf("  Difference: %zu bytes more than small test\n",
           used_large - used_small);

    /* Destroy will also print usage */
    printf("  Destroying context:\n  ");
    fcontext_destroy(ctx_large);
    printf("\n");

    /* Test 4: Manual malloc stack with alignment */
    printf("Test 4: Manual malloc stack with 16-byte alignment\n");
    size_t manual_size = 8 * 1024;
    void *manual_stack = malloc(manual_size);
    if (!manual_stack) {
        fprintf(stderr, "Failed to allocate manual stack\n");
        return 1;
    }

    /* Calculate aligned stack pointer */
    void *stack_top = (char *)manual_stack + manual_size;
    void *aligned_top = fcontext_align_stack_pointer(stack_top);

    printf("  Malloc'd stack: %p - %p\n", manual_stack, stack_top);
    printf("  Aligned top:    %p\n", aligned_top);
    printf("  Alignment:      %s\n",
           ((uintptr_t)aligned_top % FCONTEXT_STACK_ALIGNMENT == 0) ? "PASS" : "FAIL");

    free(manual_stack);
    printf("\n");

    printf("=== All tests completed ===\n");

    return 0;
}
