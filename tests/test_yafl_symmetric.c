/**
 * test_yafl_symmetric.c
 * Symmetric coroutine switching test
 *
 * Verifies that two fibers can switch back and forth to each other
 * with interleaved execution. Tests that context switches occur at least
 * twice with proper call ordering.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 */

#include "yafl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Track execution order to verify interleaving */
#define MAX_EVENTS 16
static size_t event_count = 0;
static uint32_t events[MAX_EVENTS];

/* Event identifiers */
#define EVENT_MAIN_START 1
#define EVENT_MAIN_TO_A 2
#define EVENT_A_ENTER 3
#define EVENT_A_TO_B 4
#define EVENT_B_ENTER 5
#define EVENT_B_TO_A 6
#define EVENT_A_RESUME 7
#define EVENT_A_TO_B_AGAIN 8
#define EVENT_B_RESUME 9
#define EVENT_B_TO_A_AGAIN 10
#define EVENT_A_DONE 11
#define EVENT_MAIN_END 12

/* Fiber handle for symmetric switching */
static yafl_fiber_t *fiber_b = NULL;

static void record_event(uint32_t event) {
    if(event_count < MAX_EVENTS) { events[event_count++] = event; }
    fprintf(stderr, "  [event] #%zu: %u\n", event_count, event);
    fflush(stderr);
}

/**
 * Fiber B - called by fiber A
 */
void *fiber_b_entry(void *initial_data) {
    fprintf(stderr, "[fiber_b] entered with data: %p\n", initial_data);
    fflush(stderr);
    record_event(EVENT_B_ENTER);

    /* Yield back to caller (fiber A) */
    fprintf(stderr, "[fiber_b] yielding back to A\n");
    fflush(stderr);
    record_event(EVENT_B_TO_A);
    void *resumed_data = yafl_fiber_yield((void *)(uintptr_t)0xB0B0B0B0);

    fprintf(stderr, "[fiber_b] resumed from A with data: %p\n", resumed_data);
    fflush(stderr);
    assert(resumed_data == (void *)(uintptr_t)0xA1A1A1A1);
    record_event(EVENT_B_RESUME);

    /* Yield again */
    fprintf(stderr, "[fiber_b] yielding back to A again\n");
    fflush(stderr);
    record_event(EVENT_B_TO_A_AGAIN);
    yafl_fiber_yield((void *)(uintptr_t)0xB1B1B1B1);

    fprintf(stderr, "[fiber_b] finishing\n");
    fflush(stderr);
    return (void *)(uintptr_t)0xBBBBBBBB;
}

/**
 * Fiber A - calls fiber B, then gets called back
 */
void *fiber_a_entry(void *initial_data) {
    fprintf(stderr, "[fiber_a] entered with data: %p\n", initial_data);
    fflush(stderr);
    record_event(EVENT_A_ENTER);

    assert(fiber_b != NULL);
    fprintf(stderr, "[fiber_a] have fiber_b: %p\n", (void *)fiber_b);
    fflush(stderr);

    /* First switch: call fiber B */
    fprintf(stderr, "[fiber_a] calling fiber B\n");
    fflush(stderr);
    record_event(EVENT_A_TO_B);
    void *result = yafl_fiber_switch(fiber_b, (void *)(uintptr_t)0xDEADBEEF);

    fprintf(stderr, "[fiber_a] B returned with: %p\n", result);
    fflush(stderr);
    assert(result == (void *)(uintptr_t)0xB0B0B0B0);
    record_event(EVENT_A_RESUME);

    /* Second switch: call fiber B again */
    fprintf(stderr, "[fiber_a] calling fiber B again\n");
    fflush(stderr);
    record_event(EVENT_A_TO_B_AGAIN);
    result = yafl_fiber_switch(fiber_b, (void *)(uintptr_t)0xA1A1A1A1);

    fprintf(stderr, "[fiber_a] B returned again with: %p\n", result);
    fflush(stderr);
    assert(result == (void *)(uintptr_t)0xB1B1B1B1);
    record_event(EVENT_A_DONE);

    fprintf(stderr, "[fiber_a] finishing\n");
    fflush(stderr);
    return (void *)(uintptr_t)0xAAAAAAAA;
}

int main(void) {
    fprintf(stderr, "=== yafl Symmetric Fiber Test ===\n");
    fflush(stderr);
    record_event(EVENT_MAIN_START);

    /* Create fibers */
    fprintf(stderr, "[main] creating fiber A\n");
    fflush(stderr);
    yafl_fiber_t *fiber_a = yafl_fiber_create_vmem(24 * 1024, fiber_a_entry);
    assert(fiber_a != NULL);

    fprintf(stderr, "[main] creating fiber B\n");
    fflush(stderr);
    fiber_b = yafl_fiber_create_vmem(24 * 1024, fiber_b_entry);
    assert(fiber_b != NULL);

    fprintf(stderr, "[main] entering fiber A\n");
    fflush(stderr);
    record_event(EVENT_MAIN_TO_A);
    void *result = yafl_fiber_switch(fiber_a, (void *)(uintptr_t)0xCAFEBABE);

    fprintf(stderr, "[main] fiber A finished with result: %p\n", result);
    fflush(stderr);
    assert(result == (void *)(uintptr_t)0xAAAAAAAA);

    fprintf(stderr, "[main] cleaning up\n");
    fflush(stderr);
    yafl_fiber_destroy(fiber_a);
    yafl_fiber_destroy(fiber_b);
    yafl_fiber_destroy_thread_fiber();
    record_event(EVENT_MAIN_END);

    /* Verify interleaving */
    fprintf(stderr, "\n[main] verifying execution order\n");
    fflush(stderr);
    fprintf(stderr, "[main] total events recorded: %zu\n", event_count);
    fflush(stderr);

    assert(event_count >= 10);
    fprintf(stderr, "[main] event count check passed\n");
    fflush(stderr);

    /* Verify the sequence contains the required interleaving pattern */
    bool found_a_to_b = false;
    bool found_b_to_a = false;
    bool found_a_to_b_again = false;
    bool found_b_to_a_again = false;

    for(size_t i = 0; i < event_count; i++) {
        fprintf(stderr, "[main] event[%zu] = %u\n", i, events[i]);
        fflush(stderr);

        if(events[i] == EVENT_A_TO_B) {
            found_a_to_b = true;
            fprintf(stderr, "[main] found A->B switch\n");
            fflush(stderr);
        }
        if(events[i] == EVENT_B_TO_A && found_a_to_b) {
            found_b_to_a = true;
            fprintf(stderr, "[main] found B->A switch\n");
            fflush(stderr);
        }
        if(events[i] == EVENT_A_TO_B_AGAIN && found_b_to_a) {
            found_a_to_b_again = true;
            fprintf(stderr, "[main] found A->B again switch\n");
            fflush(stderr);
        }
        if(events[i] == EVENT_B_TO_A_AGAIN && found_a_to_b_again) {
            found_b_to_a_again = true;
            fprintf(stderr, "[main] found B->A again switch\n");
            fflush(stderr);
        }
    }

    assert(found_a_to_b);
    fprintf(stderr, "[main] assertion passed: A switched to B\n");
    fflush(stderr);

    assert(found_b_to_a);
    fprintf(stderr, "[main] assertion passed: B switched to A\n");
    fflush(stderr);

    assert(found_a_to_b_again);
    fprintf(stderr, "[main] assertion passed: A switched to B again\n");
    fflush(stderr);

    assert(found_b_to_a_again);
    fprintf(stderr, "[main] assertion passed: B switched to A again\n");
    fflush(stderr);

    fprintf(stderr, "\nPASS: Symmetric fiber switching works correctly\n");
    fflush(stderr);
    return 0;
}
