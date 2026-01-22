/**
 * test_fcontext_transfer.c
 * Test data transfer through context switching
 *
 * Verifies that user data pointers are correctly passed through context switches.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "fcontext.h"

#define TEST_DATA_1 ((void *)0xDEADBEEF)
#define TEST_DATA_2 ((void *)0xCAFEBABE)
#define TEST_DATA_3 ((void *)0x12345678)

static void *received_data = NULL;

void transfer_fiber(fcontext_transfer_t t) {
    printf("  [fiber] received data: %p\n", t.data);
    fflush(stdout);
    assert(t.data == TEST_DATA_1);
    received_data = t.data;

    printf("  [fiber] sending data back: %p\n", TEST_DATA_2);
    fflush(stdout);
    t = jump_fcontext(t.prev_context, TEST_DATA_2);

    printf("  [fiber] received data again: %p\n", t.data);
    fflush(stdout);
    assert(t.data == TEST_DATA_3);
    received_data = t.data;
}

int main(void) {
    printf("=== fcontext Data Transfer Test ===\n");

    fcontext_stack_t *state = fcontext_create(24 * 1024, transfer_fiber);
    assert(state != NULL);

    /* First transfer: send TEST_DATA_1 */
    printf("[main] sending initial data: %p\n", TEST_DATA_1);
    fcontext_transfer_t t = jump_fcontext(state->context, TEST_DATA_1);

    assert(t.data == TEST_DATA_2);
    printf("[main] received data: %p\n", t.data);

    /* Resume and send TEST_DATA_3 */
    printf("[main] sending second data: %p\n", TEST_DATA_3);
    t = jump_fcontext(t.prev_context, TEST_DATA_3);

    assert(t.prev_context != NULL);
    printf("[main] fiber finished\n");

    fcontext_destroy(state);
    printf("\n✓ PASS: Data transfer works correctly\n");
    fflush(stdout);
    return 0;
}
