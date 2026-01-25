/**
 * test_fcontext_low_level.c
 * Tests the low-level API (fcontext_init) directly.
 *
 * Verifies that fcontext_init works with a manually allocated stack,
 * bypassing the fcontext_create wrapper.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static bool fiber_ran = false;

void low_level_fiber(fcontext_transfer_t t) {
    printf("  [fiber] entered low-level fiber\n");

    /* Verify data passed */
    int32_t *value = (int32_t *)t.data;
    assert(*value == 123);

    fiber_ran = true;

    printf("  [fiber] jumping back\n");
    fcontext_switch(t.prev_context, NULL);
}

int main(void) {
    printf("=== fcontext Low-Level API Test (fcontext_init) ===\n");

    /* 1. Manually allocate stack */
    size_t stack_size = 16 * 1024;
    void *stack_buffer = malloc(stack_size);
    if(!stack_buffer) {
        fprintf(stderr, "Failed to allocate stack\n");
        return 1;
    }

    /* 2. Calculate aligned stack pointer (top of stack) */
    void *sp = (char *)stack_buffer + stack_size;
    sp = fcontext_align_stack_pointer(sp);

    printf("[main] Stack allocated at %p, SP aligned to %p\n", stack_buffer, sp);

    /* 3. Create context directly */
    fcontext_t ctx = fcontext_init(sp, stack_size, low_level_fiber);
    assert(ctx != NULL);

    /* 4. Jump to context */
    int32_t data = 123;
    printf("[main] Jumping to fiber...\n");
    fcontext_switch(ctx, &data);

    printf("[main] Returned from fiber\n");
    assert(fiber_ran);

    /* 5. Cleanup */
    free(stack_buffer);

    printf("\n✓ PASS: Low-level fcontext_init test completed.\n");
    return 0;
}