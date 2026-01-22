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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fcontext.h"

static int called = 0;

void simple_fiber(fcontext_transfer_t t) {
    called = 1;
    printf("  [fiber] entry function called\n");
    /* Return to caller - will cause jump_fcontext to return */
    jump_fcontext(t.prev_context, (void *)0x42);
}

int main(void) {
    printf("=== fcontext Simple Test ===\n");

    fcontext_stack_t *state = fcontext_create(24 * 1024, simple_fiber);
    assert(state != NULL);
    printf("[main] created context with guarded stack\n");

    called = 0;
    printf("[main] entering context...\n");
    fcontext_transfer_t t = jump_fcontext(state->context, NULL);

    assert(called == 1);
    assert(t.data == (void *)0x42);
    printf("[main] context called fiber correctly\n");

    fcontext_destroy(state);
    printf("\n✓ PASS: Simple context entry works\n");
    fflush(stdout);
    return 0;
}
