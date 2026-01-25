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

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif
#include "fcontext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <malloc.h> /* for _aligned_malloc */
    #include <process.h>
    #include <windows.h>
    #if defined(_MSC_VER)
        #include <intrin.h>
    #endif
#else
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

/* Global to store stack range for verification */
static void *g_stack_base = NULL;
static size_t g_stack_size = 0;

/* Recursive function to consume stack until overflow */
fcontext_transfer_t overflow_fiber(fcontext_transfer_t t) {
    /* Allocate 1KB on stack */
    /* Force alignment to avoid potential SIGILL from SIMD instructions on unaligned stack */
#if defined(_MSC_VER)
    __declspec(align(16)) volatile char buffer[1024];
#else
    volatile char buffer[1024] __attribute__((aligned(16)));
#endif

    (void)t;

    /* Touch memory to ensure pages are committed */
    memset((void *)buffer, 0xAA, sizeof(buffer));

    /* Recurse infinitely */
    /* Use volatile to suppress -Winfinite-recursion warning */
    volatile int keep_going = 1;
    if(keep_going) { overflow_fiber(t); }

    /* Prevent tail call optimization */
    (void)buffer[0];
    return t;
}

#ifndef _WIN32
/* Signal handler to report details before dying */
void signal_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)ucontext;
    const char *name = "UNKNOWN";
    if(sig == SIGSEGV) {
        name = "SIGSEGV";
    } else if(sig == SIGBUS) {
        name = "SIGBUS";
    } else if(sig == SIGILL) {
        name = "SIGILL";
    }

    fprintf(stderr, "[child] Caught signal %d (%s)\n", sig, name);
    fprintf(stderr, "[child]   si_code: %d\n", info->si_code);
    fprintf(stderr, "[child]   si_addr: %p\n", info->si_addr);

    /* Exit with a known code to tell parent we crashed 'successfully' */
    if(sig == SIGSEGV || sig == SIGBUS || sig == SIGILL) { _exit(0); /* Success for this test means we crashed */ }
    _exit(1);
}
#endif

void do_test(void) {
    /* Create a small stack (1 page if possible) to crash quickly */
    size_t stack_size = 64 * 1024; /* 64KB to ensure we have enough room to start */

    fcontext_stack_t *ctx = fcontext_create(stack_size, overflow_fiber);
    if(!ctx) {
        fprintf(stderr, "Failed to create context\n");
        exit(1);
    }

#ifndef _WIN32
    /* Setup alternate stack for signal handler */
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    if(ss.ss_sp == NULL) {
        perror("malloc sigaltstack");
        exit(1);
    }
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if(sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack");
        exit(1);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
#endif

    g_stack_base = ctx->stack_base;
    g_stack_size = ctx->stack_size;
    printf("[child] Fiber stack: %p - %p (size: %zu)\n", g_stack_base, (char *)g_stack_base + g_stack_size, g_stack_size);

    printf("[child] Entering fiber to trigger overflow...\n");
    fflush(stdout);

    fcontext_swap(ctx->context, NULL);

    /* We should never get here if guard pages work */
    fprintf(stderr, "FAILURE: Fiber returned without crashing! Guard pages failed.\n");
    fcontext_destroy(ctx);
    exit(1);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef _WIN32
    /* Child process check for Windows */
    if(argc > 1 && strcmp(argv[1], "--child") == 0) {
        do_test();
        return 1;
    }
#endif

    printf("=== fcontext Guard Page Test ===\n");
    printf("This test intentionally overflows the stack in a child process.\n");
    printf("It expects the child to crash (SegFault/AccessViolation).\n");
    fflush(stdout);

#ifdef _WIN32
    /* Windows implementation using _spawnvp */
    const char *args[] = {argv[0], "--child", NULL};
    intptr_t ret = _spawnvp(_P_WAIT, argv[0], args);
    unsigned int exit_code = (unsigned int)ret;

    /* 0xC0000005: STATUS_ACCESS_VIOLATION */
    /* 0xC00000FD: STATUS_STACK_OVERFLOW */
    if(exit_code == 0xC0000005 || exit_code == 0xC00000FD) {
        printf("✓ PASS: Child process crashed with expected exception 0x%X\n", exit_code);
        return 0;
    }
    printf("FAILURE: Child exited with code 0x%X\n", exit_code);
    return 1;
#else
    /* POSIX implementation using fork */
    pid_t pid = fork();
    if(pid == -1) {
        perror("fork");
        return 1;
    }

    if(pid == 0) {
        /* Child process */
        do_test();
        exit(1); /* Should not be reached */
    }

    /* Parent process */
    int status;
    waitpid(pid, &status, 0);

    if(WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if(sig == SIGSEGV || sig == SIGBUS || sig == SIGILL) {
            printf("✓ PASS: Child process crashed with signal %d\n", sig);
            return 0;
        }
        printf("FAILURE: Child terminated with unexpected signal %d (%s)\n", sig, strsignal(sig));
    } else if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        /* Child caught signal and exited with 0 (see signal_handler) */
        printf("✓ PASS: Child process caught expected signal\n");
        return 0;
    } else {
        printf("FAILURE: Child did not crash (exit code %d)\n", WEXITSTATUS(status));
    }
    return 1;
#endif
}