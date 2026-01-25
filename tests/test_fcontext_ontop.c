/**
 * test_fcontext_ontop.c
 * Tests ontop_fcontext functionality.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int value;
} test_data_t;

/* Function to be executed "on top" of the fiber */
fcontext_transfer_t ontop_func(fcontext_transfer_t t) {
    printf("  [ontop] Executing ontop function\n");
    test_data_t *data = (test_data_t *)t.data;

    /* Verify we received the expected data from main */
    assert(data->value == 3);

    /* Modify data to prove we ran */
    data->value += 10;

    /* Return transfer to resume the fiber */
    return t;
}

fcontext_transfer_t fiber_func(fcontext_transfer_t t) {
    printf("  [fiber] Started\n");
    test_data_t *data = (test_data_t *)t.data;

    /* 1. Verify initial value passed from main */
    assert(data->value == 1);

    /* Modify and yield back */
    data->value = 2;
    printf("  [fiber] Yielding back to main\n");

    /* Use fcontext_swap for standard yield */
    t = fcontext_swap(t.prev_context, data);

    /* 2. We are back. ontop_fcontext should have executed ontop_func before we resumed. */
    printf("  [fiber] Resumed\n");
    data = (test_data_t *)t.data;

    /* Verify ontop_func modification (3 + 10 = 13) */
    assert(data->value == 13);

    printf("  [fiber] Finished\n");
    return fcontext_swap(t.prev_context, data);
}

int main(void) {
    printf("=== fcontext ontop Test ===\n");

    fcontext_stack_t *stack = fcontext_create(16 * 1024, fiber_func);
    if(!stack) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    test_data_t data = {.value = 1};

    printf("[main] Jumping to fiber\n");
    fcontext_transfer_t t = fcontext_swap(stack->context, &data);

    /* Fiber yielded. Value should be 2. */
    assert(data.value == 2);

    printf("[main] Fiber yielded. Calling ontop_fcontext\n");
    data.value = 3;

    /* Use ontop_fcontext to resume fiber and run ontop_func first */
    /* Note: We manually handle ASAN hooks here since ontop_fcontext is raw API */
#ifdef __SANITIZE_ADDRESS__
    void *fake_stack_save = NULL;
    __sanitizer_start_switch_fiber(&fake_stack_save, NULL, 0);
#endif
    t = ontop_fcontext(t.prev_context, &data, ontop_func);
#ifdef __SANITIZE_ADDRESS__
    __sanitizer_finish_switch_fiber(fake_stack_save, NULL, NULL);
#endif

    printf("[main] Fiber returned\n");

    fcontext_destroy(stack);

    printf("\n✓ PASS: ontop_fcontext test completed.\n");
    return 0;
}