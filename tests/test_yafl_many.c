/**
 * test_yafl_many.c
 * Scalability test with many fibers
 *
 * Verifies that 100 fibers can coexist and complete using round-robin scheduling.
 * Tests state query functions and confirms each fiber gets exactly 10 iterations.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "yafl.h"

#define NUM_FIBERS 100
#define ITERATIONS 10

static int counters[NUM_FIBERS];
static yafl_fiber_t *fibers[NUM_FIBERS];

void *fiber_entry(void *data) {
    int id = (int)(uintptr_t)data;

    fprintf(stderr, "[fiber %d] entered\n", id);
    fflush(stderr);

    /* Test yafl_fiber_current() */
    yafl_fiber_t *current = yafl_fiber_current();
    fprintf(stderr, "[fiber %d] current fiber: %p, expected: %p\n", id, (void *)current, (void *)fibers[id]);
    fflush(stderr);
    assert(current == fibers[id]);

    /* Test yafl_fiber_get_caller() */
    yafl_fiber_t *caller = yafl_fiber_get_caller();
    fprintf(stderr, "[fiber %d] caller fiber: %p\n", id, (void *)caller);
    fflush(stderr);
    assert(caller != NULL); /* Should be the thread fiber */

    for(int i = 0; i < ITERATIONS; i++) {
        fprintf(stderr, "[fiber %d] iteration %d/%d\n", id, i + 1, ITERATIONS);
        fflush(stderr);

        counters[id]++;

        /* Test yafl_fiber_get_state() on ourselves */
        yafl_fiber_state_t state = yafl_fiber_get_state(current);
        fprintf(stderr, "[fiber %d] state: %d (expected RUNNING=%d)\n", id, state, FCONTEXT_FIBER_RUNNING);
        fflush(stderr);
        assert(state == FCONTEXT_FIBER_RUNNING);

        fprintf(stderr, "[fiber %d] yielding\n", id);
        fflush(stderr);
        yafl_fiber_yield(NULL);

        fprintf(stderr, "[fiber %d] resumed\n", id);
        fflush(stderr);
    }

    fprintf(stderr, "[fiber %d] finishing with result %d\n", id, id + 1000);
    fflush(stderr);
    return (void *)(uintptr_t)(id + 1000);
}

int main(void) {
    fprintf(stderr, "=== yafl Many Fibers Test ===\n");
    fprintf(stderr, "[main] testing with %d fibers, %d iterations each\n", NUM_FIBERS, ITERATIONS);
    fflush(stderr);

    /* Test yafl_fiber_is_thread_converted() before conversion */
    bool is_converted = yafl_fiber_is_thread_converted();
    fprintf(stderr, "[main] thread converted before use: %d (expected 0)\n", is_converted);
    fflush(stderr);
    assert(!is_converted);

    /* Create 100 fibers */
    fprintf(stderr, "[main] creating %d fibers\n", NUM_FIBERS);
    fflush(stderr);
    for(int i = 0; i < NUM_FIBERS; i++) {
        counters[i] = 0;
        fibers[i] = yafl_fiber_create_vmem(24 * 1024, fiber_entry);
        assert(fibers[i] != NULL);

        /* Test yafl_fiber_get_state() on created fiber */
        yafl_fiber_state_t state = yafl_fiber_get_state(fibers[i]);
        assert(state == FCONTEXT_FIBER_CREATED);

        /* Test yafl_fiber_get_stack_size() */
        size_t stack_size = yafl_fiber_get_stack_size(fibers[i]);
        assert(stack_size > 0);

        if(i == 0 || i == NUM_FIBERS - 1) {
            fprintf(stderr, "[main] fiber %d: created, state=%d, stack_size=%zu\n", i, state, stack_size);
            fflush(stderr);
        }
    }
    fprintf(stderr, "[main] all %d fibers created successfully\n", NUM_FIBERS);
    fflush(stderr);

    /* Round-robin scheduling until all done */
    fprintf(stderr, "[main] starting round-robin scheduling\n");
    fflush(stderr);

    int round = 0;
    bool all_done = false;
    while(!all_done) {
        all_done = true;
        int active_count = 0;

        for(int i = 0; i < NUM_FIBERS; i++) {
            yafl_fiber_state_t state = yafl_fiber_get_state(fibers[i]);
            if(state != FCONTEXT_FIBER_FINISHED) {
                yafl_fiber_switch(fibers[i], (void *)(uintptr_t)i);
                all_done = false;
                active_count++;
            }
        }

        round++;
        if(round <= 3 || all_done) {
            fprintf(stderr, "[main] round %d: %d fibers still active\n", round, active_count);
            fflush(stderr);
        }
    }

    fprintf(stderr, "[main] all fibers finished after %d rounds\n", round);
    fflush(stderr);

    /* Test yafl_fiber_is_thread_converted() after implicit conversion */
    is_converted = yafl_fiber_is_thread_converted();
    fprintf(stderr, "[main] thread converted after use: %d (expected 1)\n", is_converted);
    fflush(stderr);
    assert(is_converted);

    /* Verify all fibers ran exactly ITERATIONS times */
    fprintf(stderr, "[main] verifying all fibers completed %d iterations\n", ITERATIONS);
    fflush(stderr);

    for(int i = 0; i < NUM_FIBERS; i++) {
        assert(counters[i] == ITERATIONS);

        yafl_fiber_state_t state = yafl_fiber_get_state(fibers[i]);
        assert(state == FCONTEXT_FIBER_FINISHED);

        /* Test destroy returns correct result */
        void *result = yafl_fiber_destroy(fibers[i]);
        int expected = i + 1000;
        assert(result == (void *)(uintptr_t)expected);

        if(i == 0 || i == NUM_FIBERS - 1) {
            fprintf(stderr, "[main] fiber %d: counter=%d, state=%d, result=%d\n", i, counters[i], state, (int)(uintptr_t)result);
            fflush(stderr);
        }
    }

    fprintf(stderr, "[main] all fibers verified and destroyed\n");
    fflush(stderr);

    yafl_fiber_destroy_thread_fiber();

    fprintf(stderr, "\nPASS: %d fibers x %d iterations = %d total context switches\n", NUM_FIBERS, ITERATIONS,
            NUM_FIBERS * ITERATIONS);
    fflush(stderr);
    return 0;
}
