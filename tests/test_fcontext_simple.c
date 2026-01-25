/**
 * test_fcontext_simple.c
 * Simple fcontext API test
 *
 * Verifies basic context creation and entry.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "fcontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static bool called = false;

void simple_fiber(fcontext_transfer_t t) {
    called = true;
    printf("  [fiber] entry function called\n");
    (void)t;
}

int main(void) {
    printf("=== fcontext Simple Test ===\n");

    fcontext_stack_t *state = fcontext_vmem_stack(24 * 1024);
    assert(state != NULL);
    printf("[main] allocated stack with guarded pages\n");

    state->context = fcontext_init(state->stack_top, state->stack_size, simple_fiber);
    printf("[main] initialized context\n");

    called = false;
    printf("[main] entering context...\n");
    fcontext_transfer_t t = fcontext_switch(state->context, NULL);

    assert(called);
    assert(t.data == (void *)0x42);
    printf("[main] context called fiber correctly\n");

    fcontext_stack_destroy(state);
    printf("\n✓ PASS: Simple context entry works\n");
    fflush(stdout);
    return 0;
}
