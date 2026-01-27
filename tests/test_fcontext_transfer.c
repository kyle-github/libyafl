/**
 * test_fcontext_transfer.c
 * Test data transfer through fiber switching
 *
 * Verifies that user data pointers are correctly passed through fiber switches.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "fcontext.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_DATA_1 ((void *)(uintptr_t)0xDEADBEEF)
#define TEST_DATA_2 ((void *)(uintptr_t)0xCAFEBABE)
#define TEST_DATA_3 ((void *)(uintptr_t)0x12345678)

void *transfer_fiber(void *initial_data) {
    fprintf(stderr, "[fiber] received initial data: %p\n", initial_data);
    fflush(stderr);
    assert(initial_data == TEST_DATA_1);

    fprintf(stderr, "[fiber] yielding back: %p\n", TEST_DATA_2);
    fflush(stderr);
    void *data = fcontext_fiber_yield(TEST_DATA_2);

    fprintf(stderr, "[fiber] received again: %p\n", data);
    fflush(stderr);
    assert(data == TEST_DATA_3);

    return (void *)0x9999;
}

int main(void) {
    fprintf(stderr, "=== fcontext Data Transfer Test ===\n");
    fflush(stderr);

    fcontext_fiber_t *fiber = fcontext_fiber_create_vmem(24 * 1024, transfer_fiber);
    assert(fiber != NULL);

    fprintf(stderr, "[main] sending initial data: %p\n", TEST_DATA_1);
    fflush(stderr);
    void *result = fcontext_fiber_switch(fiber, TEST_DATA_1);

    assert(result == TEST_DATA_2);
    fprintf(stderr, "[main] received data: %p\n", result);
    fflush(stderr);

    fprintf(stderr, "[main] sending second data: %p\n", TEST_DATA_3);
    fflush(stderr);
    result = fcontext_fiber_switch(fiber, TEST_DATA_3);

    fprintf(stderr, "[main] fiber finished: %p\n", result);
    fflush(stderr);

    fcontext_fiber_destroy(fiber);
    fcontext_fiber_destroy_thread_fiber();

    fprintf(stderr, "\n✓ PASS: Data transfer works correctly\n");
    fflush(stderr);
    return 0;
}
