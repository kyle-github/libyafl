/**
 * test_fcontext_guard.c
 * Verify hardware guard pages catch stack overflow
 *
 * This test intentionally overflows the stack to trigger a guard page fault.
 * It is expected to CRASH (Segmentation Fault or Access Violation).
 * The test runner should mark this as PASS if it crashes (WILL_FAIL property).
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fcontext.h"

/* Recursive function to consume stack until overflow */
void overflow_fiber(fcontext_transfer_t t) {
    /* Allocate 1KB on stack */
    volatile char buffer[1024];

    /* Touch memory to ensure pages are committed */
    memset((void*)buffer, 0xAA, sizeof(buffer));

    /* Print progress every 4KB (approx 4 frames) to show we are running */
    static int depth = 0;
    depth += sizeof(buffer);
    if (depth % 4096 == 0) {
        printf("  [fiber] Stack depth: %d bytes\n", depth);
        fflush(stdout);
    }

    /* Recurse infinitely */
    /* Use volatile to suppress -Winfinite-recursion warning */
    volatile int keep_going = 1;
    if (keep_going) {
        overflow_fiber(t);
    }

    /* Prevent tail call optimization */
    (void)buffer[0];
}

int main(void) {
    printf("=== fcontext Guard Page Test ===\n");
    printf("This test intentionally overflows the stack.\n");
    printf("It is EXPECTED TO CRASH (SegFault/AccessViolation).\n");
    fflush(stdout);

    /* Create a small stack (1 page if possible) to crash quickly */
    size_t stack_size = 4096;

    fcontext_stack_t *ctx = fcontext_create(stack_size, overflow_fiber);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    printf("[main] Entering fiber to trigger overflow...\n");
    fflush(stdout);

    fcontext_swap(ctx->context, NULL);

    /* We should never get here if guard pages work */
    fprintf(stderr, "FAILURE: Fiber returned without crashing! Guard pages failed.\n");
    fcontext_destroy(ctx);

    /* Return 0 (Success) to indicate failure to CMake because WILL_FAIL=TRUE */
    return 0;
}