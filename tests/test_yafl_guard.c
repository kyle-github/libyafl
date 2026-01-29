/*
 * test_yafl_guard.c - Guard page overflow detection test
 *
 * Verifies that guard pages detect stack overflow.
 * Uses alternate signal stack on POSIX and SEH on Windows.
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/yafl.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <signal.h>
    #include <unistd.h>
#endif

static volatile bool fault_caught = false;
static volatile bool in_overflow_test = false;
static yafl_fiber_t *g_fiber = NULL;

#ifndef _WIN32
/* POSIX signal handler */
static void segv_handler(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;

    fprintf(stderr, "[handler] caught signal %d\n", sig);
    fflush(stderr);

    if (in_overflow_test && g_fiber != NULL) {
        fault_caught = true;
        fprintf(stderr, "[handler] stack overflow detected as expected\n");
        fflush(stderr);

        /* Exit the fiber - suspend with error code */
        yafl_fiber_suspend((void *)0xDEADDEAD);
    } else {
        fprintf(stderr, "[handler] unexpected signal outside test\n");
        fflush(stderr);
        exit(1);
    }
}
#endif

/* Recursive function to overflow the stack */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
#endif
static void overflow_stack(int depth) {
    volatile char buffer[1024];

    /* Touch the buffer to prevent optimization */
    memset((void *)buffer, 0xAA, sizeof(buffer));

    fprintf(stderr, "[overflow] depth=%d, buffer=%p\n", depth, (void *)buffer);
    fflush(stderr);

    /* Recurse until we hit the guard page */
    overflow_stack(depth + 1);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static void *guard_test_fiber(void *data) {
    (void)data;

    fprintf(stderr, "[fiber] starting guard page test\n");
    fflush(stderr);

#ifdef _WIN32
    __try {
        in_overflow_test = true;
        overflow_stack(0);
    } __except(GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        fprintf(stderr, "[fiber] caught EXCEPTION_STACK_OVERFLOW\n");
        fflush(stderr);
        fault_caught = true;
        in_overflow_test = false;
        return (void *)0x1;
    }
#else
    in_overflow_test = true;
    overflow_stack(0);
#endif

    fprintf(stderr, "[fiber] ERROR: should not reach here\n");
    fflush(stderr);
    in_overflow_test = false;
    return (void *)0x0;
}

int main(void) {
    fprintf(stderr, "=== yafl Guard Page Test ===\n");
    fflush(stderr);

#ifndef _WIN32
    /* Set up alternate signal stack (required on POSIX) */
    size_t alt_stack_size = SIGSTKSZ * 4;
    void *alt_stack = malloc(alt_stack_size);
    assert(alt_stack != NULL);

    stack_t ss;
    ss.ss_sp = alt_stack;
    ss.ss_size = alt_stack_size;
    ss.ss_flags = 0;

    if (sigaltstack(&ss, NULL) == -1) {
        fprintf(stderr, "[main] ERROR: sigaltstack failed\n");
        fflush(stderr);
        free(alt_stack);
        return 1;
    }

    fprintf(stderr, "[main] alternate signal stack installed: %p (size %zu)\n", alt_stack,
            alt_stack_size);
    fflush(stderr);

    /* Install signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "[main] ERROR: sigaction SIGSEGV failed\n");
        fflush(stderr);
        free(alt_stack);
        return 1;
    }

    if (sigaction(SIGBUS, &sa, NULL) == -1) {
        fprintf(stderr, "[main] ERROR: sigaction SIGBUS failed\n");
        fflush(stderr);
        free(alt_stack);
        return 1;
    }

    fprintf(stderr, "[main] signal handlers installed\n");
    fflush(stderr);
#endif

    /* Create fiber with vmem (has guard pages) */
    fprintf(stderr, "[main] creating fiber with guard pages\n");
    fflush(stderr);

    yafl_fiber_t *fiber = yafl_fiber_create(guard_test_fiber, 24 * 1024,
                                            YAFL_STACK_FLAGS_VMEM);
    assert(fiber != NULL);
    g_fiber = fiber;

    /* Run the fiber - it will overflow and trigger the guard page */
    fprintf(stderr, "[main] resuming fiber\n");
    fflush(stderr);

    void *result = yafl_fiber_resume(fiber, NULL);

    fprintf(stderr, "[main] fiber returned: %p\n", result);
    fflush(stderr);

    /* Verify the fault was caught */
    if (!fault_caught) {
        fprintf(stderr, "[main] FAIL: guard page fault was not caught\n");
        fflush(stderr);
        yafl_fiber_destroy(fiber);
#ifndef _WIN32
        free(alt_stack);
#endif
        return 1;
    }

#ifdef _WIN32
    /* On Windows, the fiber returned normally after catching the exception */
    assert(result == (void *)0x1);
#else
    /* On POSIX, the fiber suspended from the signal handler */
    assert(result == (void *)0xDEADDEAD);
#endif

    fprintf(stderr, "[main] cleaning up\n");
    fflush(stderr);

    yafl_fiber_destroy(fiber);

#ifndef _WIN32
    /* Restore default signal handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    /* Disable alternate stack */
    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, NULL);

    free(alt_stack);
#endif

    fprintf(stderr, "\nPASS: Guard page detected stack overflow\n");
    fflush(stderr);
    return 0;
}
